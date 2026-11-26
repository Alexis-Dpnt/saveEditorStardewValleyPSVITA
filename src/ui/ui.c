/*
 * ui.c — Interface utilisateur maison (SDL2 + SDL2_ttf) en 960x544 :
 * - Onglet 0 : Sélecteur de sauvegardes (scan automatique)
 * - Onglet 1 : Joueur (argent, énergie, santé)
 * - Onglet 2 : Compétences (5 niveaux)
 * - Onglet 3 : Ferme & Temps (jour, saison, année)
 * - Onglet 4 : Bâtiments + mini-carte enrichie (coffres, arroseurs, machines,
 *              arbres, cultures) avec ajout, déplacement et suppression.
 */

#include "ui/ui.h"

#include <stdio.h>
#include <string.h>

#define TAB_COUNT 5

static const char *TAB_NAMES[TAB_COUNT] = {
    "Sauvegardes",
    "Joueur",
    "Competences",
    "Ferme & Temps",
    "Batiments"
};

/* Nombre de lignes dans les onglets liste (1, 2, 3) */
static int tab_row_count(int tab) {
    switch (tab) {
        case 1: return 3; /* Argent, Énergie max, Santé max */
        case 2: return 5; /* Agriculture, Minage, Cueillette, Pêche, Combat */
        case 3: return 3; /* Jour, Saison, Année */
        default: return 0;
    }
}

/* Borne un entier `v` entre `min_v` et `max_v` inclus */
static int clamp_int(int v, int min_v, int max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

/* Affiche un message temporaire en bas de l'écran pendant 3 secondes */
static void set_status(UIState *ui, const char *msg) {
    snprintf(ui->status_msg, sizeof(ui->status_msg), "%s", msg);
    ui->status_until_ms = SDL_GetTicks() + 3000;
}

/* Dessine du texte UTF-8 à (x, y) */
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
 * Retourne l'index dans `save->buildings[]` du N-ième bâtiment actif (non supprimé),
 * ou -1 s'il n'y en a pas.
 */
static int nth_active_building(const SaveData *save, int nth) {
    int seen = 0;
    for (int i = 0; i < save->building_count; i++) {
        if (!save->buildings[i].is_deleted) {
            if (seen == nth) {
                return i;
            }
            seen++;
        }
    }
    return -1;
}

/* Initialise les polices, scanne les sauvegardes disponibles et place le curseur par défaut */
bool ui_init(UIState *ui, const char *font_path) {
    memset(ui, 0, sizeof(*ui));

    ui->font_title = TTF_OpenFont(font_path, 24);
    ui->font_body  = TTF_OpenFont(font_path, 18);
    ui->font_small = TTF_OpenFont(font_path, 14);

    if (!ui->font_title || !ui->font_body || !ui->font_small) {
        fprintf(stderr, "Erreur chargement police (%s): %s\n", font_path, TTF_GetError());
        return false;
    }

    ui->save_file_count = platform_scan_saves(ui->save_files, MAX_SAVE_FILES);
    ui->active_tab = 1; /* Démarre sur Joueur (Sauvegardes accessible via Q/L) */
    ui->bld_mode = BLD_MODE_ADD_SELECT;
    ui->bld_catalog_idx = 0;
    ui->bld_existing_idx = 0;
    ui->cursor_x = 35;
    ui->cursor_y = 25;
    return true;
}

void ui_destroy(UIState *ui) {
    if (ui->font_title) TTF_CloseFont(ui->font_title);
    if (ui->font_body)  TTF_CloseFont(ui->font_body);
    if (ui->font_small) TTF_CloseFont(ui->font_small);
}

/* Modifie une valeur dans les onglets 1 (Joueur), 2 (Compétences) ou 3 (Ferme & Temps) */
static void adjust_value(SaveData *save, int tab, int row, int delta) {
    if (tab == 1) {
        if (row == 0) save->money = clamp_int(save->money + delta * 1000, 0, 99999999);
        if (row == 1) save->max_stamina = clamp_int(save->max_stamina + delta * 10, 100, 999);
        if (row == 2) save->max_health = clamp_int(save->max_health + delta * 10, 50, 999);
    } else if (tab == 2) {
        if (row == 0) save->farming_level  = clamp_int(save->farming_level + delta, 0, 10);
        if (row == 1) save->mining_level   = clamp_int(save->mining_level + delta, 0, 10);
        if (row == 2) save->foraging_level = clamp_int(save->foraging_level + delta, 0, 10);
        if (row == 3) save->fishing_level  = clamp_int(save->fishing_level + delta, 0, 10);
        if (row == 4) save->combat_level   = clamp_int(save->combat_level + delta, 0, 10);
    } else if (tab == 3) {
        if (row == 0) save->day_of_month = clamp_int(save->day_of_month + delta, 1, 28);
        if (row == 1) save->season_index = (save->season_index + delta + 4) % 4;
        if (row == 2) save->year = clamp_int(save->year + delta, 1, 99);
    }
    save->dirty = true;
}

/* Gère les entrées dans l'onglet 0 : Sélecteur de sauvegardes */
static void handle_saves_input(UIState *ui, SaveData *save, InputAction action) {
    if (ui->save_file_count <= 0) {
        return;
    }
    if (action == INPUT_UP) {
        ui->selected_row = (ui->selected_row - 1 + ui->save_file_count) % ui->save_file_count;
    } else if (action == INPUT_DOWN) {
        ui->selected_row = (ui->selected_row + 1) % ui->save_file_count;
    } else if (action == INPUT_CONFIRM) {
        const SaveFileEntry *entry = &ui->save_files[ui->selected_row];
        if (save_data_load_xml(save, entry->path)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Sauvegarde chargee : %s (%d batiments)",
                     entry->label, save_data_active_building_count(save));
            set_status(ui, msg);
            ui->bld_existing_idx = 0;
        } else {
            set_status(ui, "Erreur lors du chargement du fichier XML.");
        }
    }
}

/* Gère les entrées dans l'onglet 4 : Bâtiments (Ajouter, Déplacer, Supprimer) */
static void handle_buildings_input(UIState *ui, SaveData *save, InputAction action) {
    int active_count = save_data_active_building_count(save);

    /* Bascule rapide Tab / Carré entre "Ajouter" et "Gérer existants" */
    if (action == INPUT_MODE_SWITCH) {
        if (ui->bld_mode == BLD_MODE_ADD_SELECT || ui->bld_mode == BLD_MODE_ADD_PLACE) {
            ui->bld_mode = BLD_MODE_MANAGE_LIST;
            ui->bld_existing_idx = 0;
        } else {
            ui->bld_mode = BLD_MODE_ADD_SELECT;
        }
        return;
    }

    if (ui->bld_mode == BLD_MODE_ADD_SELECT) {
        const BuildingBlueprint *bp = &BUILDING_CATALOG[ui->bld_catalog_idx];
        if (action == INPUT_UP) {
            ui->bld_catalog_idx = (ui->bld_catalog_idx - 1 + BUILDING_CATALOG_COUNT) % BUILDING_CATALOG_COUNT;
        } else if (action == INPUT_DOWN) {
            ui->bld_catalog_idx = (ui->bld_catalog_idx + 1) % BUILDING_CATALOG_COUNT;
        } else if (action == INPUT_CONFIRM) {
            ui->bld_mode = BLD_MODE_ADD_PLACE;
            ui->cursor_x = clamp_int(ui->cursor_x, 0, FARM_WIDTH - bp->tiles_wide);
            ui->cursor_y = clamp_int(ui->cursor_y, 0, FARM_HEIGHT - bp->tiles_high);
        }
    } else if (ui->bld_mode == BLD_MODE_ADD_PLACE) {
        const BuildingBlueprint *bp = &BUILDING_CATALOG[ui->bld_catalog_idx];
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
                snprintf(msg, sizeof(msg), "%s ajoute en (%d, %d) ! F5/Start pour sauver.",
                         bp->display_name, ui->cursor_x, ui->cursor_y);
                set_status(ui, msg);
            } else {
                set_status(ui, "Zone bloquee ou deja occupee.");
            }
        } else if (action == INPUT_CANCEL) {
            ui->bld_mode = BLD_MODE_ADD_SELECT;
        }
    } else if (ui->bld_mode == BLD_MODE_MANAGE_LIST) {
        if (active_count <= 0) {
            return;
        }
        if (ui->bld_existing_idx >= active_count) {
            ui->bld_existing_idx = active_count - 1;
        }
        int real_idx = nth_active_building(save, ui->bld_existing_idx);

        if (action == INPUT_UP) {
            ui->bld_existing_idx = (ui->bld_existing_idx - 1 + active_count) % active_count;
        } else if (action == INPUT_DOWN) {
            ui->bld_existing_idx = (ui->bld_existing_idx + 1) % active_count;
        } else if (action == INPUT_CONFIRM && real_idx >= 0) {
            /* Passe en mode déplacement pour ce bâtiment */
            ui->cursor_x = save->buildings[real_idx].tile_x;
            ui->cursor_y = save->buildings[real_idx].tile_y;
            ui->bld_mode = BLD_MODE_MANAGE_MOVE;
        } else if (action == INPUT_DELETE && real_idx >= 0) {
            char name[32];
            snprintf(name, sizeof(name), "%s", save->buildings[real_idx].building_type);
            if (save_data_delete_building(save, real_idx)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "%s supprime ! F5/Start pour enregistrer.", name);
                set_status(ui, msg);
                int new_count = save_data_active_building_count(save);
                if (ui->bld_existing_idx >= new_count && new_count > 0) {
                    ui->bld_existing_idx = new_count - 1;
                }
            }
        }
    } else if (ui->bld_mode == BLD_MODE_MANAGE_MOVE) {
        int real_idx = nth_active_building(save, ui->bld_existing_idx);
        if (real_idx < 0) {
            ui->bld_mode = BLD_MODE_MANAGE_LIST;
            return;
        }
        const BuildingEntry *b = &save->buildings[real_idx];
        if (action == INPUT_UP) {
            ui->cursor_y = clamp_int(ui->cursor_y - 1, 0, FARM_HEIGHT - b->tiles_high);
        } else if (action == INPUT_DOWN) {
            ui->cursor_y = clamp_int(ui->cursor_y + 1, 0, FARM_HEIGHT - b->tiles_high);
        } else if (action == INPUT_LEFT) {
            ui->cursor_x = clamp_int(ui->cursor_x - 1, 0, FARM_WIDTH - b->tiles_wide);
        } else if (action == INPUT_RIGHT) {
            ui->cursor_x = clamp_int(ui->cursor_x + 1, 0, FARM_WIDTH - b->tiles_wide);
        } else if (action == INPUT_CONFIRM) {
            if (save_data_move_building(save, real_idx, ui->cursor_x, ui->cursor_y)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "%s deplace en (%d, %d) ! F5/Start pour sauver.",
                         b->building_type, ui->cursor_x, ui->cursor_y);
                set_status(ui, msg);
                ui->bld_mode = BLD_MODE_MANAGE_LIST;
            } else {
                set_status(ui, "Impossible de deplacer ici : zone bloquee.");
            }
        } else if (action == INPUT_CANCEL) {
            ui->bld_mode = BLD_MODE_MANAGE_LIST;
        }
    }
}

void ui_handle_input(UIState *ui, SaveData *save, InputAction action) {
    if (action == INPUT_TAB_PREV) {
        ui->active_tab = (ui->active_tab - 1 + TAB_COUNT) % TAB_COUNT;
        ui->selected_row = 0;
        if (ui->active_tab == 0) {
            ui->save_file_count = platform_scan_saves(ui->save_files, MAX_SAVE_FILES);
        }
        return;
    }
    if (action == INPUT_TAB_NEXT) {
        ui->active_tab = (ui->active_tab + 1) % TAB_COUNT;
        ui->selected_row = 0;
        if (ui->active_tab == 0) {
            ui->save_file_count = platform_scan_saves(ui->save_files, MAX_SAVE_FILES);
        }
        return;
    }
    if (action == INPUT_SAVE) {
        if (save_data_save_xml(save)) {
            set_status(ui, "Sauvegarde + SaveGameInfo enregistres (+ backup .bak) !");
        } else {
            set_status(ui, "Mode demo : charge d'abord un fichier dans l'onglet Sauvegardes.");
        }
        return;
    }

    if (ui->active_tab == 0) {
        handle_saves_input(ui, save, action);
        return;
    }
    if (ui->active_tab == 4) {
        handle_buildings_input(ui, save, action);
        return;
    }

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

/* Libellé + valeur pour les onglets 1, 2 et 3 */
static void get_row_display(const SaveData *save, int tab, int row, const char **label, char *value, size_t value_size) {
    if (tab == 1) {
        if (row == 0) { *label = "Argent (po)";   snprintf(value, value_size, "%d g", save->money); }
        if (row == 1) { *label = "Energie max";   snprintf(value, value_size, "%d", save->max_stamina); }
        if (row == 2) { *label = "Sante max";     snprintf(value, value_size, "%d", save->max_health); }
    } else if (tab == 2) {
        if (row == 0) { *label = "Agriculture";   snprintf(value, value_size, "Niv. %d / 10", save->farming_level); }
        if (row == 1) { *label = "Minage";        snprintf(value, value_size, "Niv. %d / 10", save->mining_level); }
        if (row == 2) { *label = "Cueillette";    snprintf(value, value_size, "Niv. %d / 10", save->foraging_level); }
        if (row == 3) { *label = "Peche";         snprintf(value, value_size, "Niv. %d / 10", save->fishing_level); }
        if (row == 4) { *label = "Combat";        snprintf(value, value_size, "Niv. %d / 10", save->combat_level); }
    } else if (tab == 3) {
        if (row == 0) { *label = "Jour";          snprintf(value, value_size, "%d / 28", save->day_of_month); }
        if (row == 1) { *label = "Saison";        snprintf(value, value_size, "%s", save_data_season_name(save->season_index)); }
        if (row == 2) { *label = "Annee";         snprintf(value, value_size, "An %d", save->year); }
    }
}

/* Rendu de l'onglet 0 : Sélecteur de sauvegardes */
static void render_saves_tab(UIState *ui, SDL_Renderer *renderer, const SaveData *save) {
    SDL_Color col_gold  = { 248, 206, 102, 255 };
    SDL_Color col_text  = { 245, 236, 220, 255 };
    SDL_Color col_muted = { 185, 165, 145, 255 };
    SDL_Color col_green = { 120, 225, 120, 255 };

    char title[128];
    snprintf(title, sizeof(title), "Sauvegardes trouvees dans '%s' (%d) :",
             platform_get_saves_dir(), ui->save_file_count);
    draw_text(renderer, ui->font_body, title, 44, 154, col_gold);

    if (ui->save_file_count == 0) {
        draw_text(renderer, ui->font_body, "Aucune sauvegarde trouvee.", 44, 210, col_muted);
        draw_text(renderer, ui->font_small,
                  "Place tes sauvegardes decryptees dans le dossier ci-dessus puis relance.",
                  44, 245, col_muted);
        return;
    }

    const int visible = 6;
    int start = ui->selected_row - visible / 2;
    if (ui->save_file_count > visible) {
        start = clamp_int(start, 0, ui->save_file_count - visible);
    } else {
        start = 0;
    }

    int count = ui->save_file_count < visible ? ui->save_file_count : visible;
    for (int i = 0; i < count; i++) {
        int idx = start + i;
        const SaveFileEntry *entry = &ui->save_files[idx];
        bool sel = (idx == ui->selected_row);
        bool is_current = (strcmp(entry->path, save->file_path) == 0);

        SDL_Rect row = { 44, 192 + i * 44, SCREEN_WIDTH - 88, 38 };
        if (sel) {
            SDL_SetRenderDrawColor(renderer, 96, 64, 40, 255);
            SDL_RenderFillRect(renderer, &row);
            SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
            SDL_RenderDrawRect(renderer, &row);
        }

        draw_text(renderer, ui->font_body, entry->label, row.x + 14, row.y + 8,
                  sel ? col_gold : col_text);
        draw_text(renderer, ui->font_small, is_current ? "[CHARGEE]" : entry->path,
                  row.x + 280, row.y + 11, is_current ? col_green : col_muted);
    }
}

/* Rendu de l'onglet 4 : Mini-carte enrichie + Ajout / Déplacement / Suppression de bâtiments */
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

    /* 1. Terrain de base + couche d'objets/arbres/cultures de la sauvegarde */
    for (int y = 0; y < FARM_HEIGHT; y++) {
        for (int x = 0; x < FARM_WIDTH; x++) {
            uint8_t t = STANDARD_FARM_GRID[y][x];
            uint8_t ov = save->farm_overlay[y][x];

            if (t == TILE_FREE) {
                SDL_SetRenderDrawColor(renderer, 118, 152, 74, 255);
            } else if (t == TILE_NO_BUILD) {
                SDL_SetRenderDrawColor(renderer, 96, 86, 68, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 46, 58, 72, 255);
            }

            /* Superpose les éléments dynamiques de la sauvegarde */
            if (ov == OVERLAY_CROP) {
                SDL_SetRenderDrawColor(renderer, 120, 84, 52, 255);   /* Cultures / terre labourée */
            } else if (ov == OVERLAY_TREE) {
                SDL_SetRenderDrawColor(renderer, 42, 92, 38, 255);    /* Arbres */
            } else if (ov == OVERLAY_OTHER_OBJ) {
                SDL_SetRenderDrawColor(renderer, 145, 138, 125, 255); /* Pierres / bois / clôtures */
            } else if (ov == OVERLAY_MACHINE) {
                SDL_SetRenderDrawColor(renderer, 225, 130, 45, 255);  /* Machines */
            } else if (ov == OVERLAY_SPRINKLER) {
                SDL_SetRenderDrawColor(renderer, 70, 215, 235, 255);  /* Arroseurs */
            } else if (ov == OVERLAY_CHEST) {
                SDL_SetRenderDrawColor(renderer, 250, 215, 55, 255);  /* Coffres */
            }

            SDL_Rect tile_rect = { map_x + x * cell, map_y + y * cell, cell, cell };
            SDL_RenderFillRect(renderer, &tile_rect);
        }
    }

    int selected_real = (ui->bld_mode == BLD_MODE_MANAGE_LIST || ui->bld_mode == BLD_MODE_MANAGE_MOVE)
                        ? nth_active_building(save, ui->bld_existing_idx)
                        : -1;

    /* 2. Bâtiments actifs sur la mini-carte */
    for (int i = 0; i < save->building_count; i++) {
        const BuildingEntry *b = &save->buildings[i];
        if (b->is_deleted) {
            continue;
        }
        /* Pendant un déplacement, on dessine le fantôme à la place de l'ancienne position */
        if (ui->bld_mode == BLD_MODE_MANAGE_MOVE && i == selected_real) {
            continue;
        }
        SDL_Rect br = {
            map_x + b->tile_x * cell,
            map_y + b->tile_y * cell,
            b->tiles_wide * cell,
            b->tiles_high * cell
        };
        if (i == selected_real) {
            SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255); /* Sélectionné : doré */
        } else if (b->is_new) {
            SDL_SetRenderDrawColor(renderer, 58, 195, 108, 255);  /* Nouveau : vert vif */
        } else if (b->is_moved) {
            SDL_SetRenderDrawColor(renderer, 180, 120, 230, 255); /* Déplacé : violet */
        } else {
            SDL_SetRenderDrawColor(renderer, 74, 146, 206, 255);  /* Existant : bleu */
        }
        SDL_RenderFillRect(renderer, &br);
        SDL_SetRenderDrawColor(renderer, 250, 250, 250, 255);
        SDL_RenderDrawRect(renderer, &br);
    }

    /* 3. Curseur fantôme (ajout OU déplacement) */
    bool placing = (ui->bld_mode == BLD_MODE_ADD_PLACE || ui->bld_mode == BLD_MODE_MANAGE_MOVE);
    int ghost_w = 0, ghost_h = 0;
    bool valid_spot = false;

    if (ui->bld_mode == BLD_MODE_ADD_PLACE) {
        const BuildingBlueprint *bp = &BUILDING_CATALOG[ui->bld_catalog_idx];
        ghost_w = bp->tiles_wide;
        ghost_h = bp->tiles_high;
        valid_spot = save_data_can_place_building(save, ui->cursor_x, ui->cursor_y, ghost_w, ghost_h);
    } else if (ui->bld_mode == BLD_MODE_MANAGE_MOVE && selected_real >= 0) {
        const BuildingEntry *b = &save->buildings[selected_real];
        ghost_w = b->tiles_wide;
        ghost_h = b->tiles_high;
        valid_spot = save_data_can_place_building_ignore(save, ui->cursor_x, ui->cursor_y,
                                                         ghost_w, ghost_h, selected_real);
    }

    if (placing) {
        SDL_Rect ghost = {
            map_x + ui->cursor_x * cell,
            map_y + ui->cursor_y * cell,
            ghost_w * cell,
            ghost_h * cell
        };
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        if (valid_spot) {
            SDL_SetRenderDrawColor(renderer, 80, 240, 100, 165);
        } else {
            SDL_SetRenderDrawColor(renderer, 240, 70, 70, 175);
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

    /* 4. Panneau de droite */
    const int rx = 454;
    int active_count = save_data_active_building_count(save);
    bool is_add_mode = (ui->bld_mode == BLD_MODE_ADD_SELECT || ui->bld_mode == BLD_MODE_ADD_PLACE);

    /* Switch visuel [Ajouter] / [Gerer existants] */
    SDL_Rect pill_add = { rx, 146, 224, 28 };
    SDL_Rect pill_mng = { rx + 234, 146, 236, 28 };
    SDL_SetRenderDrawColor(renderer, is_add_mode ? 150 : 62, is_add_mode ? 95 : 44, is_add_mode ? 42 : 34, 255);
    SDL_RenderFillRect(renderer, &pill_add);
    SDL_SetRenderDrawColor(renderer, !is_add_mode ? 150 : 62, !is_add_mode ? 95 : 44, !is_add_mode ? 42 : 34, 255);
    SDL_RenderFillRect(renderer, &pill_mng);

    draw_text(renderer, ui->font_small, "Ajouter (Tab/Carre)", pill_add.x + 12, pill_add.y + 6,
              is_add_mode ? col_gold : col_muted);
    char mng_label[48];
    snprintf(mng_label, sizeof(mng_label), "Existants (%d) [Tab]", active_count);
    draw_text(renderer, ui->font_small, mng_label, pill_mng.x + 12, pill_mng.y + 6,
              !is_add_mode ? col_gold : col_muted);

    if (ui->bld_mode == BLD_MODE_ADD_SELECT) {
        const int visible = 7;
        int start = clamp_int(ui->bld_catalog_idx - visible / 2, 0, BUILDING_CATALOG_COUNT - visible);
        for (int i = 0; i < visible; i++) {
            int idx = start + i;
            const BuildingBlueprint *item = &BUILDING_CATALOG[idx];
            SDL_Rect row = { rx, 184 + i * 31, 470, 27 };
            bool sel = (idx == ui->bld_catalog_idx);
            if (sel) {
                SDL_SetRenderDrawColor(renderer, 96, 64, 40, 255);
                SDL_RenderFillRect(renderer, &row);
                SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
                SDL_RenderDrawRect(renderer, &row);
            }
            char line[96];
            snprintf(line, sizeof(line), "%s  (%dx%d)", item->display_name, item->tiles_wide, item->tiles_high);
            draw_text(renderer, ui->font_small, line, row.x + 10, row.y + 5, sel ? col_gold : col_text);
        }
        draw_text(renderer, ui->font_small, "Entree/Croix: Placer  |  Tab/Carre: Gerer existants", rx, 412, col_green);
    } else if (ui->bld_mode == BLD_MODE_ADD_PLACE) {
        const BuildingBlueprint *bp = &BUILDING_CATALOG[ui->bld_catalog_idx];
        char l1[96], l2[96], l3[96];
        snprintf(l1, sizeof(l1), "Ajouter  : %s", bp->display_name);
        snprintf(l2, sizeof(l2), "Taille   : %d x %d cases", bp->tiles_wide, bp->tiles_high);
        snprintf(l3, sizeof(l3), "Position : X = %d , Y = %d", ui->cursor_x, ui->cursor_y);

        draw_text(renderer, ui->font_body, l1, rx, 194, col_text);
        draw_text(renderer, ui->font_body, l2, rx, 224, col_text);
        draw_text(renderer, ui->font_body, l3, rx, 254, col_gold);
        draw_text(renderer, ui->font_body,
                  valid_spot ? "Emplacement : VALIDE" : "Emplacement : BLOQUE",
                  rx, 290, valid_spot ? col_green : col_red);
        draw_text(renderer, ui->font_small, "Fleches: Deplacer  |  Entree: Poser  |  Backspace: Retour", rx, 344, col_muted);
    } else if (ui->bld_mode == BLD_MODE_MANAGE_LIST) {
        if (active_count == 0) {
            draw_text(renderer, ui->font_body, "Aucun batiment sur cette ferme.", rx, 210, col_muted);
        } else {
            const int visible = 7;
            int start = 0;
            if (active_count > visible) {
                start = clamp_int(ui->bld_existing_idx - visible / 2, 0, active_count - visible);
            }
            int show = active_count < visible ? active_count : visible;
            for (int i = 0; i < show; i++) {
                int nth = start + i;
                int real_idx = nth_active_building(save, nth);
                const BuildingEntry *b = &save->buildings[real_idx];
                SDL_Rect row = { rx, 184 + i * 31, 470, 27 };
                bool sel = (nth == ui->bld_existing_idx);
                if (sel) {
                    SDL_SetRenderDrawColor(renderer, 96, 64, 40, 255);
                    SDL_RenderFillRect(renderer, &row);
                    SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
                    SDL_RenderDrawRect(renderer, &row);
                }
                char line[96];
                snprintf(line, sizeof(line), "%s  (%d,%d) %dx%d%s",
                         b->building_type, b->tile_x, b->tile_y, b->tiles_wide, b->tiles_high,
                         b->is_new ? " [NEW]" : (b->is_moved ? " [DEPL]" : ""));
                draw_text(renderer, ui->font_small, line, row.x + 10, row.y + 5, sel ? col_gold : col_text);
            }
            draw_text(renderer, ui->font_small, "Entree/Croix: Deplacer  |  X/Triangle: Supprimer", rx, 412, col_green);
        }
    } else if (ui->bld_mode == BLD_MODE_MANAGE_MOVE && selected_real >= 0) {
        const BuildingEntry *b = &save->buildings[selected_real];
        char l1[96], l2[96];
        snprintf(l1, sizeof(l1), "Deplacer : %s (%dx%d)", b->building_type, b->tiles_wide, b->tiles_high);
        snprintf(l2, sizeof(l2), "Nouvelle pos : X = %d , Y = %d", ui->cursor_x, ui->cursor_y);

        draw_text(renderer, ui->font_body, l1, rx, 198, col_text);
        draw_text(renderer, ui->font_body, l2, rx, 232, col_gold);
        draw_text(renderer, ui->font_body,
                  valid_spot ? "Emplacement : VALIDE" : "Emplacement : BLOQUE",
                  rx, 270, valid_spot ? col_green : col_red);
        draw_text(renderer, ui->font_small, "Fleches: Deplacer  |  Entree: Valider  |  Backspace: Annuler", rx, 326, col_muted);
    }

    /* Légende de la mini-carte (bas du panneau droit) */
    draw_text(renderer, ui->font_small,
              "Carte: Bleu=Bat.  Jaune=Coffre  Cyan=Arros.  Orange=Mach.  Vert=Arbre",
              rx, 448, col_muted);
}

void ui_render(UIState *ui, SDL_Renderer *renderer, const SaveData *save) {
    /* 1. Fond bois sombre */
    SDL_SetRenderDrawColor(renderer, 36, 26, 22, 255);
    SDL_RenderClear(renderer);

    /* 2. Header */
    SDL_Rect header = { 0, 0, SCREEN_WIDTH, 68 };
    SDL_SetRenderDrawColor(renderer, 64, 42, 30, 255);
    SDL_RenderFillRect(renderer, &header);

    SDL_Color col_gold   = { 248, 206, 102, 255 };
    SDL_Color col_text   = { 245, 236, 220, 255 };
    SDL_Color col_muted  = { 185, 165, 145, 255 };
    SDL_Color col_accent = { 130, 210, 115, 255 };

    draw_text(renderer, ui->font_title, "Stardew Valley Save Editor", 24, 20, col_gold);

    char subtitle[160];
    snprintf(subtitle, sizeof(subtitle), "%s — Ferme %s%s",
             save->farmer_name, save->farm_name, save->dirty ? " *" : "");
    draw_text(renderer, ui->font_small, subtitle, 560, 26, col_text);

    /* 3. Barre de 5 onglets calés sur 960 px */
    int tab_w = 174;
    int tab_x = 25;
    for (int i = 0; i < TAB_COUNT; i++) {
        SDL_Rect tab_rect = { tab_x + i * (tab_w + 10), 84, tab_w, 42 };
        if (i == ui->active_tab) {
            SDL_SetRenderDrawColor(renderer, 166, 104, 46, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 58, 42, 34, 255);
        }
        SDL_RenderFillRect(renderer, &tab_rect);
        draw_text(renderer, ui->font_body, TAB_NAMES[i], tab_rect.x + 12, tab_rect.y + 10,
                  i == ui->active_tab ? col_gold : col_muted);
    }

    /* 4. Panneau principal */
    SDL_Rect panel = { 24, 140, SCREEN_WIDTH - 48, 338 };
    SDL_SetRenderDrawColor(renderer, 50, 36, 28, 255);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 118, 82, 54, 255);
    SDL_RenderDrawRect(renderer, &panel);

    if (ui->active_tab == 0) {
        render_saves_tab(ui, renderer, save);
    } else if (ui->active_tab == 4) {
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

            draw_text(renderer, ui->font_body, label, row_rect.x + 20, row_rect.y + 12, col_text);

            char decorated_val[80];
            if (selected) {
                snprintf(decorated_val, sizeof(decorated_val), "<  %s  >", val_buf);
            } else {
                snprintf(decorated_val, sizeof(decorated_val), "%s", val_buf);
            }
            draw_text(renderer, ui->font_body, decorated_val, row_rect.x + 560, row_rect.y + 12,
                      selected ? col_gold : col_muted);
        }
    }

    /* 5. Message de statut temporaire */
    if (ui->status_msg[0] != '\0' && SDL_GetTicks() < ui->status_until_ms) {
        draw_text(renderer, ui->font_small, ui->status_msg, 24, 484, col_accent);
    }

    /* 6. Footer */
    SDL_Rect footer = { 0, SCREEN_HEIGHT - 38, SCREEN_WIDTH, 38 };
    SDL_SetRenderDrawColor(renderer, 26, 18, 15, 255);
    SDL_RenderFillRect(renderer, &footer);

    const char *help_text = (ui->active_tab == 4)
        ? "Fleches: Deplacer  |  Entree: Valider  |  Tab/Carre: Mode  |  X/Tri: Suppr  |  Q/E: Onglets  |  F5: Sauver"
        : "Haut/Bas: Naviguer  |  Gauche/Droite: Modifier  |  Entree: Charger  |  Q/E (L/R): Onglets  |  F5: Sauver";
    draw_text(renderer, ui->font_small, help_text, 24, SCREEN_HEIGHT - 27, col_muted);

    SDL_RenderPresent(renderer);
}
