/*
 * ui.c — Implémentation de l'interface utilisateur maison (SDL2 + SDL2_ttf).
 * Gère les 4 onglets (Joueur, Compétences, Ferme & Temps, Bâtiments),
 * la mini-carte interactive 80x65 et le rendu 2D en 960x544.
 */

#include "ui/ui.h"

#include <stdio.h>
#include <string.h>

/* Nombre total d'onglets affichés en haut de l'interface */
#define TAB_COUNT 4

/* Libellés des onglets dans l'ordre d'affichage */
static const char *TAB_NAMES[TAB_COUNT] = {
    "Joueur",
    "Competences",
    "Ferme & Temps",
    "Batiments"
};

/*
 * Retourne le nombre de lignes éditables dans les onglets liste (0, 1, 2).
 */
static int tab_row_count(int tab) {
    switch (tab) {
        case 0: return 3; /* Argent, Énergie max, Santé max */
        case 1: return 5; /* Agriculture, Minage, Cueillette, Pêche, Combat */
        case 2: return 3; /* Jour, Saison, Année */
        default: return 0;
    }
}

/*
 * Borne un entier `v` entre `min_v` et `max_v` inclus.
 */
static int clamp_int(int v, int min_v, int max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

/*
 * Affiche un message temporaire en bas de l'écran pendant 3 secondes (3000 ms).
 */
static void set_status(UIState *ui, const char *msg) {
    snprintf(ui->status_msg, sizeof(ui->status_msg), "%s", msg);
    ui->status_until_ms = SDL_GetTicks() + 3000;
}

/*
 * Dessine une chaîne UTF-8 à la position `(x, y)` avec la police `font` et la couleur `color`.
 */
static void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    if (!font || !text || text[0] == '\0') {
        return;
    }
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) {
        return;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = { x, y, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

/*
 * Initialise l'état de l'interface et charge la police en 3 tailles (titre, corps, petit).
 */
bool ui_init(UIState *ui, const char *font_path) {
    memset(ui, 0, sizeof(*ui));

    ui->font_title = TTF_OpenFont(font_path, 26);
    ui->font_body  = TTF_OpenFont(font_path, 20);
    ui->font_small = TTF_OpenFont(font_path, 15);

    if (!ui->font_title || !ui->font_body || !ui->font_small) {
        fprintf(stderr, "Erreur chargement police (%s): %s\n", font_path, TTF_GetError());
        return false;
    }

    /* Position initiale du curseur au centre de la ferme */
    ui->bld_mode = BLD_MODE_SELECT;
    ui->bld_catalog_idx = 0;
    ui->cursor_x = 35;
    ui->cursor_y = 25;
    return true;
}

/*
 * Ferme et libère les 3 polices TrueType chargées à l'initialisation.
 */
void ui_destroy(UIState *ui) {
    if (ui->font_title) TTF_CloseFont(ui->font_title);
    if (ui->font_body)  TTF_CloseFont(ui->font_body);
    if (ui->font_small) TTF_CloseFont(ui->font_small);
}

/*
 * Modifie la valeur du champ correspondant à `(tab, row)` selon `delta` (-1 ou +1).
 */
static void adjust_value(SaveData *save, int tab, int row, int delta) {
    if (tab == 0) {
        if (row == 0) save->money = clamp_int(save->money + delta * 1000, 0, 99999999);
        if (row == 1) save->max_stamina = clamp_int(save->max_stamina + delta * 10, 100, 999);
        if (row == 2) save->max_health = clamp_int(save->max_health + delta * 10, 50, 999);
    } else if (tab == 1) {
        if (row == 0) save->farming_level  = clamp_int(save->farming_level + delta, 0, 10);
        if (row == 1) save->mining_level   = clamp_int(save->mining_level + delta, 0, 10);
        if (row == 2) save->foraging_level = clamp_int(save->foraging_level + delta, 0, 10);
        if (row == 3) save->fishing_level  = clamp_int(save->fishing_level + delta, 0, 10);
        if (row == 4) save->combat_level   = clamp_int(save->combat_level + delta, 0, 10);
    } else if (tab == 2) {
        if (row == 0) save->day_of_month = clamp_int(save->day_of_month + delta, 1, 28);
        if (row == 1) save->season_index = (save->season_index + delta + 4) % 4;
        if (row == 2) save->year = clamp_int(save->year + delta, 1, 99);
    }
    save->dirty = true;
}

/*
 * Gère les entrées spécifiques à l'onglet Bâtiments (tab == 3) :
 * - BLD_MODE_SELECT : choisir un bâtiment dans le catalogue (↑/↓) puis Entrée/Croix pour placer
 * - BLD_MODE_PLACE  : déplacer le bâtiment sur la mini-carte (↑↓←→), Entrée/Croix pour poser,
 *                     Backspace/Rond pour revenir au catalogue
 */
static void handle_buildings_input(UIState *ui, SaveData *save, InputAction action) {
    const BuildingBlueprint *bp = &BUILDING_CATALOG[ui->bld_catalog_idx];

    if (ui->bld_mode == BLD_MODE_SELECT) {
        if (action == INPUT_UP) {
            ui->bld_catalog_idx = (ui->bld_catalog_idx - 1 + BUILDING_CATALOG_COUNT) % BUILDING_CATALOG_COUNT;
        } else if (action == INPUT_DOWN) {
            ui->bld_catalog_idx = (ui->bld_catalog_idx + 1) % BUILDING_CATALOG_COUNT;
        } else if (action == INPUT_CONFIRM) {
            ui->bld_mode = BLD_MODE_PLACE;
            /* Recale le curseur pour que le bâtiment reste dans les limites 80x65 */
            ui->cursor_x = clamp_int(ui->cursor_x, 0, FARM_WIDTH - bp->tiles_wide);
            ui->cursor_y = clamp_int(ui->cursor_y, 0, FARM_HEIGHT - bp->tiles_high);
        }
    } else {
        if (action == INPUT_UP) {
            ui->cursor_y = clamp_int(ui->cursor_y - 1, 0, FARM_HEIGHT - bp->tiles_high);
        } else if (action == INPUT_DOWN) {
            ui->cursor_y = clamp_int(ui->cursor_y + 1, 0, FARM_HEIGHT - bp->tiles_high);
        } else if (action == INPUT_LEFT) {
            ui->cursor_x = clamp_int(ui->cursor_x - 1, 0, FARM_WIDTH - bp->tiles_wide);
        } else if (action == INPUT_RIGHT) {
            ui->cursor_x = clamp_int(ui->cursor_x + 1, 0, FARM_WIDTH - bp->tiles_wide);
        } else if (action == INPUT_CONFIRM) {
            if (save_data_add_building(save, ui->bld_catalog_idx, ui->cursor_x, ui->cursor_y)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "%s ajoute en (%d, %d) ! Appuie sur F5/Start pour sauvegarder.",
                         bp->display_name, ui->cursor_x, ui->cursor_y);
                set_status(ui, msg);
            } else {
                set_status(ui, "Impossible de construire ici : zone bloquee ou deja occupee.");
            }
        } else if (action == INPUT_CANCEL) {
            ui->bld_mode = BLD_MODE_SELECT;
        }
    }
}

/*
 * Réagit à une action utilisateur (`InputAction`) pour tous les onglets.
 */
void ui_handle_input(UIState *ui, SaveData *save, InputAction action) {
    /* Changement d'onglet et sauvegarde globaux */
    if (action == INPUT_TAB_PREV) {
        ui->active_tab = (ui->active_tab - 1 + TAB_COUNT) % TAB_COUNT;
        ui->selected_row = 0;
        ui->bld_mode = BLD_MODE_SELECT;
        return;
    }
    if (action == INPUT_TAB_NEXT) {
        ui->active_tab = (ui->active_tab + 1) % TAB_COUNT;
        ui->selected_row = 0;
        ui->bld_mode = BLD_MODE_SELECT;
        return;
    }
    if (action == INPUT_SAVE) {
        if (save_data_save_xml(save)) {
            set_status(ui, "Sauvegarde enregistree (+ backup .bak cree) !");
        } else {
            set_status(ui, "Mode demo : aucun fichier XML charge a sauvegarder.");
        }
        return;
    }

    /* Onglet Bâtiments (tab == 3) */
    if (ui->active_tab == 3) {
        handle_buildings_input(ui, save, action);
        return;
    }

    /* Onglets classiques en liste (tabs 0, 1, 2) */
    int rows = tab_row_count(ui->active_tab);
    if (rows <= 0) {
        return;
    }
    switch (action) {
        case INPUT_UP:
            ui->selected_row = (ui->selected_row - 1 + rows) % rows;
            break;
        case INPUT_DOWN:
            ui->selected_row = (ui->selected_row + 1) % rows;
            break;
        case INPUT_LEFT:
            adjust_value(save, ui->active_tab, ui->selected_row, -1);
            break;
        case INPUT_RIGHT:
            adjust_value(save, ui->active_tab, ui->selected_row, 1);
            break;
        default:
            break;
    }
}

/*
 * Prépare le libellé (`*label`) et le texte formaté de la valeur (`value`)
 * pour une ligne donnée `(tab, row)` des onglets 0, 1 et 2.
 */
static void get_row_display(const SaveData *save, int tab, int row, const char **label, char *value, size_t value_size) {
    if (tab == 0) {
        if (row == 0) { *label = "Argent (po)";   snprintf(value, value_size, "%d g", save->money); }
        if (row == 1) { *label = "Energie max";   snprintf(value, value_size, "%d", save->max_stamina); }
        if (row == 2) { *label = "Sante max";     snprintf(value, value_size, "%d", save->max_health); }
    } else if (tab == 1) {
        if (row == 0) { *label = "Agriculture";   snprintf(value, value_size, "Niv. %d / 10", save->farming_level); }
        if (row == 1) { *label = "Minage";        snprintf(value, value_size, "Niv. %d / 10", save->mining_level); }
        if (row == 2) { *label = "Cueillette";    snprintf(value, value_size, "Niv. %d / 10", save->foraging_level); }
        if (row == 3) { *label = "Peche";         snprintf(value, value_size, "Niv. %d / 10", save->fishing_level); }
        if (row == 4) { *label = "Combat";        snprintf(value, value_size, "Niv. %d / 10", save->combat_level); }
    } else if (tab == 2) {
        if (row == 0) { *label = "Jour";          snprintf(value, value_size, "%d / 28", save->day_of_month); }
        if (row == 1) { *label = "Saison";        snprintf(value, value_size, "%s", save_data_season_name(save->season_index)); }
        if (row == 2) { *label = "Annee";         snprintf(value, value_size, "An %d", save->year); }
    }
}

/*
 * Dessine l'onglet Bâtiments (tab == 3) :
 * - À gauche : mini-carte 80x65 (5 px par case = 400x325 px) avec terrain, bâtiments
 *   existants, nouveaux bâtiments et curseur de placement vert/rouge.
 * - À droite : catalogue des 16 bâtiments PS Vita + infos de position et statut.
 */
static void render_buildings_tab(UIState *ui, SDL_Renderer *renderer, const SaveData *save) {
    const int cell = 5;
    const int map_x = 34;
    const int map_y = 146;
    const int map_w = FARM_WIDTH * cell;  /* 400 px */
    const int map_h = FARM_HEIGHT * cell; /* 325 px */

    SDL_Color col_gold   = { 248, 206, 102, 255 };
    SDL_Color col_text   = { 245, 236, 220, 255 };
    SDL_Color col_muted  = { 185, 165, 145, 255 };
    SDL_Color col_green  = { 120, 225, 120, 255 };
    SDL_Color col_red    = { 235, 100, 100, 255 };

    /* 1. Terrain de la ferme 80x65 depuis STANDARD_FARM_GRID */
    for (int y = 0; y < FARM_HEIGHT; y++) {
        for (int x = 0; x < FARM_WIDTH; x++) {
            uint8_t t = STANDARD_FARM_GRID[y][x];
            if (t == TILE_FREE) {
                SDL_SetRenderDrawColor(renderer, 118, 152, 74, 255);  /* Herbe / terre constructible */
            } else if (t == TILE_NO_BUILD) {
                SDL_SetRenderDrawColor(renderer, 96, 86, 68, 255);    /* Chemins / bordures */
            } else {
                SDL_SetRenderDrawColor(renderer, 46, 58, 72, 255);    /* Eau / falaises / maison */
            }
            SDL_Rect tile_rect = { map_x + x * cell, map_y + y * cell, cell, cell };
            SDL_RenderFillRect(renderer, &tile_rect);
        }
    }

    /* 2. Bâtiments existants (bleu-cyan) et nouvellement ajoutés (vert vif) */
    for (int i = 0; i < save->building_count; i++) {
        const BuildingEntry *b = &save->buildings[i];
        SDL_Rect br = {
            map_x + b->tile_x * cell,
            map_y + b->tile_y * cell,
            b->tiles_wide * cell,
            b->tiles_high * cell
        };
        if (b->is_new) {
            SDL_SetRenderDrawColor(renderer, 58, 195, 108, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 74, 146, 206, 255);
        }
        SDL_RenderFillRect(renderer, &br);
        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        SDL_RenderDrawRect(renderer, &br);
    }

    /* 3. Curseur fantôme du bâtiment en mode PLACE */
    const BuildingBlueprint *bp = &BUILDING_CATALOG[ui->bld_catalog_idx];
    bool valid_spot = save_data_can_place_building(save, ui->cursor_x, ui->cursor_y,
                                                   bp->tiles_wide, bp->tiles_high);
    if (ui->bld_mode == BLD_MODE_PLACE) {
        SDL_Rect ghost = {
            map_x + ui->cursor_x * cell,
            map_y + ui->cursor_y * cell,
            bp->tiles_wide * cell,
            bp->tiles_high * cell
        };
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        if (valid_spot) {
            SDL_SetRenderDrawColor(renderer, 80, 240, 100, 150);
        } else {
            SDL_SetRenderDrawColor(renderer, 240, 70, 70, 165);
        }
        SDL_RenderFillRect(renderer, &ghost);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &ghost);
    }

    /* Cadre autour de la mini-carte */
    SDL_Rect map_border = { map_x - 1, map_y - 1, map_w + 2, map_h + 2 };
    SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
    SDL_RenderDrawRect(renderer, &map_border);

    /* 4. Panneau de droite : catalogue ou informations de placement */
    const int rx = 456;
    char header_info[96];
    snprintf(header_info, sizeof(header_info), "Batiments sur la ferme : %d / %d",
             save->building_count, MAX_BUILDINGS);
    draw_text(renderer, ui->font_small, header_info, rx, 148, col_muted);

    if (ui->bld_mode == BLD_MODE_SELECT) {
        draw_text(renderer, ui->font_body, "1. Choisis un batiment :", rx, 170, col_gold);

        /* Liste défilante de 8 éléments visibles parmi les 16 du catalogue */
        const int visible = 8;
        int start = ui->bld_catalog_idx - visible / 2;
        start = clamp_int(start, 0, BUILDING_CATALOG_COUNT - visible);

        for (int i = 0; i < visible; i++) {
            int idx = start + i;
            const BuildingBlueprint *item = &BUILDING_CATALOG[idx];
            SDL_Rect row = { rx, 202 + i * 31, 464, 28 };
            bool sel = (idx == ui->bld_catalog_idx);

            if (sel) {
                SDL_SetRenderDrawColor(renderer, 96, 64, 40, 255);
                SDL_RenderFillRect(renderer, &row);
                SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
                SDL_RenderDrawRect(renderer, &row);
            }

            char line[96];
            snprintf(line, sizeof(line), "%s  (%dx%d)", item->display_name, item->tiles_wide, item->tiles_high);
            draw_text(renderer, ui->font_small, line, row.x + 12, row.y + 5, sel ? col_gold : col_text);
        }

        draw_text(renderer, ui->font_small, "Entree / Croix : Placer sur la carte", rx, 454, col_green);
    } else {
        draw_text(renderer, ui->font_body, "2. Place le batiment sur la carte :", rx, 174, col_gold);

        char line1[96], line2[96], line3[96];
        snprintf(line1, sizeof(line1), "Batiment : %s", bp->display_name);
        snprintf(line2, sizeof(line2), "Taille   : %d x %d cases", bp->tiles_wide, bp->tiles_high);
        snprintf(line3, sizeof(line3), "Position : X = %d , Y = %d", ui->cursor_x, ui->cursor_y);

        draw_text(renderer, ui->font_body, line1, rx, 214, col_text);
        draw_text(renderer, ui->font_body, line2, rx, 246, col_text);
        draw_text(renderer, ui->font_body, line3, rx, 278, col_gold);

        draw_text(renderer, ui->font_body,
                  valid_spot ? "Emplacement : VALIDE" : "Emplacement : BLOQUE",
                  rx, 320, valid_spot ? col_green : col_red);

        draw_text(renderer, ui->font_small, "Fleches / D-Pad  : Deplacer le batiment", rx, 372, col_muted);
        draw_text(renderer, ui->font_small, "Entree / Croix   : Poser le batiment", rx, 396, col_green);
        draw_text(renderer, ui->font_small, "Backspace / Rond : Changer de batiment", rx, 420, col_muted);
        draw_text(renderer, ui->font_small, "F5 / Start       : Enregistrer dans le XML", rx, 444, col_gold);
    }
}

/*
 * Dessine une frame complète de l'interface en 960x544.
 */
void ui_render(UIState *ui, SDL_Renderer *renderer, const SaveData *save) {
    /* 1. Fond bois sombre */
    SDL_SetRenderDrawColor(renderer, 36, 26, 22, 255);
    SDL_RenderClear(renderer);

    /* 2. Bandeau header */
    SDL_Rect header = { 0, 0, SCREEN_WIDTH, 68 };
    SDL_SetRenderDrawColor(renderer, 64, 42, 30, 255);
    SDL_RenderFillRect(renderer, &header);

    SDL_Color col_gold   = { 248, 206, 102, 255 };
    SDL_Color col_text   = { 245, 236, 220, 255 };
    SDL_Color col_muted  = { 185, 165, 145, 255 };
    SDL_Color col_accent = { 130, 210, 115, 255 };

    draw_text(renderer, ui->font_title, "Stardew Valley Save Editor", 24, 18, col_gold);

    char subtitle[160];
    snprintf(subtitle, sizeof(subtitle), "%s — Ferme %s%s",
             save->farmer_name, save->farm_name, save->dirty ? " *" : "");
    draw_text(renderer, ui->font_small, subtitle, 560, 26, col_text);

    /* 3. Barre d'onglets (4 onglets calés sur 960 px) */
    int tab_w = 219;
    int tab_x = 24;
    for (int i = 0; i < TAB_COUNT; i++) {
        SDL_Rect tab_rect = { tab_x + i * (tab_w + 12), 84, tab_w, 42 };
        if (i == ui->active_tab) {
            SDL_SetRenderDrawColor(renderer, 166, 104, 46, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 58, 42, 34, 255);
        }
        SDL_RenderFillRect(renderer, &tab_rect);
        draw_text(renderer, ui->font_body, TAB_NAMES[i], tab_rect.x + 16, tab_rect.y + 9,
                  i == ui->active_tab ? col_gold : col_muted);
    }

    /* 4. Panneau principal */
    SDL_Rect panel = { 24, 140, SCREEN_WIDTH - 48, 338 };
    SDL_SetRenderDrawColor(renderer, 50, 36, 28, 255);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 118, 82, 54, 255);
    SDL_RenderDrawRect(renderer, &panel);

    if (ui->active_tab == 3) {
        render_buildings_tab(ui, renderer, save);
    } else {
        int rows = tab_row_count(ui->active_tab);
        for (int r = 0; r < rows; r++) {
            SDL_Rect row_rect = { 44, 162 + r * 56, SCREEN_WIDTH - 88, 46 };
            bool selected = (r == ui->selected_row);

            if (selected) {
                SDL_SetRenderDrawColor(renderer, 96, 64, 40, 255);
                SDL_RenderFillRect(renderer, &row_rect);
                SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
                SDL_RenderDrawRect(renderer, &row_rect);
            }

            const char *label = "";
            char val_buf[64] = "";
            get_row_display(save, ui->active_tab, r, &label, val_buf, sizeof(val_buf));

            draw_text(renderer, ui->font_body, label, row_rect.x + 20, row_rect.y + 11, col_text);

            char decorated_val[80];
            if (selected) {
                snprintf(decorated_val, sizeof(decorated_val), "<  %s  >", val_buf);
            } else {
                snprintf(decorated_val, sizeof(decorated_val), "%s", val_buf);
            }
            draw_text(renderer, ui->font_body, decorated_val, row_rect.x + 560, row_rect.y + 11,
                      selected ? col_gold : col_muted);
        }
    }

    /* 5. Message de statut temporaire */
    if (ui->status_msg[0] != '\0' && SDL_GetTicks() < ui->status_until_ms) {
        draw_text(renderer, ui->font_small, ui->status_msg, 24, 484, col_accent);
    }

    /* 6. Bandeau footer avec rappel des contrôles */
    SDL_Rect footer = { 0, SCREEN_HEIGHT - 38, SCREEN_WIDTH, 38 };
    SDL_SetRenderDrawColor(renderer, 26, 18, 15, 255);
    SDL_RenderFillRect(renderer, &footer);

    const char *help_text = (ui->active_tab == 3)
        ? "Fleches: Naviguer/Deplacer  |  Entree: Choisir/Poser  |  Backspace: Retour  |  Q/E: Onglets  |  F5: Sauver  |  Esc: Quitter"
        : "Haut/Bas: Naviguer  |  Gauche/Droite: Modifier  |  Q/E (L/R): Onglets  |  F5 (Start): Sauvegarder  |  Esc: Quitter";
    draw_text(renderer, ui->font_small, help_text, 24, SCREEN_HEIGHT - 28, col_muted);

    SDL_RenderPresent(renderer);
}
