/*
 * ui.c — Interface utilisateur maison (SDL2 + SDL2_ttf) en 960x544 :
 * - Onglet 0 : Sélecteur de sauvegardes (scan automatique)
 * - Onglet 1 : Joueur (argent, énergie, santé)
 * - Onglet 2 : Compétences (5 niveaux)
 * - Onglet 3 : Ferme & Temps (jour, saison, année)
 * - Onglet 4 : Bâtiments + mini-carte enrichie (ajout, déplacement, suppression)
 * - Onglet 5 : Objets (Inventaire joueur par défaut, coffres réels, catalogue PS Vita 1.2)
 */

#include "ui/ui.h"

#include <stdio.h>
#include <string.h>

#define TAB_COUNT 6

static const char *TAB_NAMES[TAB_COUNT] = {
    "Sauvegardes",
    "Joueur",
    "Competences",
    "Ferme & Temps",
    "Batiments",
    "Objets"
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

/* Retourne l'index dans `save->buildings[]` du N-ième bâtiment actif */
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

/* Compte le nombre d'objets dans ITEMS_DB correspondant à la catégorie `cat` (0 = Tous) */
static int items_db_filtered_count(int cat) {
    if (cat == 0) {
        return ITEMS_DB_COUNT;
    }
    int count = 0;
    for (int i = 0; i < ITEMS_DB_COUNT; i++) {
        if (ITEMS_DB[i].ui_category == cat) {
            count++;
        }
    }
    return count;
}

/* Retourne l'index réel dans ITEMS_DB du N-ième élément filtré */
static int items_db_nth(int cat, int nth) {
    int seen = 0;
    for (int i = 0; i < ITEMS_DB_COUNT; i++) {
        if (cat == 0 || ITEMS_DB[i].ui_category == cat) {
            if (seen == nth) {
                return i;
            }
            seen++;
        }
    }
    return -1;
}

/* Initialise les polices, scanne les sauvegardes disponibles et initialise l'état UI */
bool ui_init(UIState *ui, const char *font_path) {
    memset(ui, 0, sizeof(*ui));

    ui->font_title = TTF_OpenFont(font_path, 24);
    ui->font_body  = TTF_OpenFont(font_path, 16);
    ui->font_small = TTF_OpenFont(font_path, 14);

    if (!ui->font_title || !ui->font_body || !ui->font_small) {
        fprintf(stderr, "Erreur chargement police (%s): %s\n", font_path, TTF_GetError());
        return false;
    }

    ui->save_file_count = platform_scan_saves(ui->save_files, MAX_SAVE_FILES);
    ui->active_tab = 5; /* Ouvre directement sur l'onglet Objets pour tester immédiatement */
    ui->bld_mode = BLD_MODE_ADD_SELECT;
    ui->bld_catalog_idx = 0;
    ui->bld_existing_idx = 0;
    ui->cursor_x = 35;
    ui->cursor_y = 25;

    ui->items_focus = ITEMS_FOCUS_STORAGE;
    ui->items_storage_idx = 0; /* Par défaut : Inventaire Joueur */
    ui->items_slot_idx = 0;
    ui->items_category_idx = 0;
    ui->items_catalog_cursor = 0;
    ui->items_add_qty = 1;
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
            snprintf(msg, sizeof(msg), "Chargee : %s (%d bat., %d coffres)",
                     entry->label, save_data_active_building_count(save),
                     save->storage_count > 0 ? save->storage_count - 1 : 0);
            set_status(ui, msg);
            ui->bld_existing_idx = 0;
            ui->items_storage_idx = 0;
            ui->items_slot_idx = 0;
        } else {
            set_status(ui, "Erreur lors du chargement du fichier XML.");
        }
    }
}

/* Gère les entrées dans l'onglet 4 : Bâtiments */
static void handle_buildings_input(UIState *ui, SaveData *save, InputAction action) {
    int active_count = save_data_active_building_count(save);

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

/*
 * Gère les entrées de l'onglet 5 : Objets & Stockages branché sur SaveData + ITEMS_DB.
 */
static void handle_items_input(UIState *ui, SaveData *save, InputAction action) {
    if (save->storage_count <= 0) {
        return;
    }
    if (ui->items_storage_idx >= save->storage_count) {
        ui->items_storage_idx = 0;
        ui->items_slot_idx = 0;
    }

    if (action == INPUT_MODE_SWITCH) {
        if (ui->items_focus == ITEMS_FOCUS_STORAGE) {
            ui->items_focus = ITEMS_FOCUS_CATALOG;
        } else {
            ui->items_focus = ITEMS_FOCUS_STORAGE;
        }
        return;
    }

    StorageContainer *st = &save->storages[ui->items_storage_idx];

    if (ui->items_focus == ITEMS_FOCUS_STORAGE) {
        if (action == INPUT_LEFT) {
            ui->items_storage_idx = (ui->items_storage_idx - 1 + save->storage_count) % save->storage_count;
            ui->items_slot_idx = 0;
        } else if (action == INPUT_RIGHT) {
            ui->items_storage_idx = (ui->items_storage_idx + 1) % save->storage_count;
            ui->items_slot_idx = 0;
        } else if (action == INPUT_UP) {
            ui->items_slot_idx = (ui->items_slot_idx - 1 + st->max_slots) % st->max_slots;
        } else if (action == INPUT_DOWN) {
            ui->items_slot_idx = (ui->items_slot_idx + 1) % st->max_slots;
        } else if (action == INPUT_CONFIRM) {
            ui->items_focus = ITEMS_FOCUS_CATALOG;
        } else if (action == INPUT_DELETE) {
            char removed_name[64];
            snprintf(removed_name, sizeof(removed_name), "%s", st->slots[ui->items_slot_idx].name);
            if (save_data_remove_item(save, ui->items_storage_idx, ui->items_slot_idx)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "%s retire de %s ! F5/Start pour sauver.",
                         removed_name, st->label);
                set_status(ui, msg);
            } else {
                set_status(ui, "Ce slot est deja vide.");
            }
        }
    } else if (ui->items_focus == ITEMS_FOCUS_CATALOG) {
        int filtered = items_db_filtered_count(ui->items_category_idx);
        if (action == INPUT_LEFT) {
            ui->items_category_idx = (ui->items_category_idx - 1 + ITEM_CAT_COUNT) % ITEM_CAT_COUNT;
            ui->items_catalog_cursor = 0;
        } else if (action == INPUT_RIGHT) {
            ui->items_category_idx = (ui->items_category_idx + 1) % ITEM_CAT_COUNT;
            ui->items_catalog_cursor = 0;
        } else if (action == INPUT_UP && filtered > 0) {
            ui->items_catalog_cursor = (ui->items_catalog_cursor - 1 + filtered) % filtered;
        } else if (action == INPUT_DOWN && filtered > 0) {
            ui->items_catalog_cursor = (ui->items_catalog_cursor + 1) % filtered;
        } else if (action == INPUT_CONFIRM && filtered > 0) {
            ui->items_focus = ITEMS_FOCUS_QTY;
        } else if (action == INPUT_CANCEL) {
            ui->items_focus = ITEMS_FOCUS_STORAGE;
        }
    } else if (ui->items_focus == ITEMS_FOCUS_QTY) {
        if (action == INPUT_LEFT) {
            ui->items_add_qty = clamp_int(ui->items_add_qty - 1, 1, 999);
        } else if (action == INPUT_RIGHT) {
            ui->items_add_qty = clamp_int(ui->items_add_qty + 1, 1, 999);
        } else if (action == INPUT_DOWN) {
            ui->items_add_qty = clamp_int(ui->items_add_qty - 10, 1, 999);
        } else if (action == INPUT_UP) {
            ui->items_add_qty = clamp_int(ui->items_add_qty + 10, 1, 999);
        } else if (action == INPUT_CONFIRM) {
            int db_idx = items_db_nth(ui->items_category_idx, ui->items_catalog_cursor);
            if (db_idx >= 0) {
                if (save_data_add_item(save, ui->items_storage_idx, db_idx, ui->items_add_qty)) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "%s x%d ajoute dans %s ! F5/Start pour sauver.",
                             ITEMS_DB[db_idx].name, ui->items_add_qty, st->label);
                    set_status(ui, msg);
                } else {
                    set_status(ui, "Stockage plein ! Libere un slot ou change de coffre.");
                }
            }
            ui->items_focus = ITEMS_FOCUS_CATALOG;
        } else if (action == INPUT_CANCEL) {
            ui->items_focus = ITEMS_FOCUS_CATALOG;
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
    if (ui->active_tab == 5) {
        handle_items_input(ui, save, action);
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

        draw_text(renderer, ui->font_body, entry->label, row.x + 14, row.y + 9,
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
    const int map_w = FARM_WIDTH * cell;
    const int map_h = FARM_HEIGHT * cell;

    SDL_Color col_gold   = { 248, 206, 102, 255 };
    SDL_Color col_text   = { 245, 236, 220, 255 };
    SDL_Color col_muted  = { 185, 165, 145, 255 };
    SDL_Color col_green  = { 120, 225, 120, 255 };
    SDL_Color col_red    = { 235, 100, 100, 255 };

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

            if (ov == OVERLAY_CROP) {
                SDL_SetRenderDrawColor(renderer, 120, 84, 52, 255);
            } else if (ov == OVERLAY_TREE) {
                SDL_SetRenderDrawColor(renderer, 42, 92, 38, 255);
            } else if (ov == OVERLAY_OTHER_OBJ) {
                SDL_SetRenderDrawColor(renderer, 145, 138, 125, 255);
            } else if (ov == OVERLAY_MACHINE) {
                SDL_SetRenderDrawColor(renderer, 225, 130, 45, 255);
            } else if (ov == OVERLAY_SPRINKLER) {
                SDL_SetRenderDrawColor(renderer, 70, 215, 235, 255);
            } else if (ov == OVERLAY_CHEST) {
                SDL_SetRenderDrawColor(renderer, 250, 215, 55, 255);
            }

            SDL_Rect tile_rect = { map_x + x * cell, map_y + y * cell, cell, cell };
            SDL_RenderFillRect(renderer, &tile_rect);
        }
    }

    int selected_real = (ui->bld_mode == BLD_MODE_MANAGE_LIST || ui->bld_mode == BLD_MODE_MANAGE_MOVE)
                        ? nth_active_building(save, ui->bld_existing_idx)
                        : -1;

    for (int i = 0; i < save->building_count; i++) {
        const BuildingEntry *b = &save->buildings[i];
        if (b->is_deleted || (ui->bld_mode == BLD_MODE_MANAGE_MOVE && i == selected_real)) {
            continue;
        }
        SDL_Rect br = {
            map_x + b->tile_x * cell,
            map_y + b->tile_y * cell,
            b->tiles_wide * cell,
            b->tiles_high * cell
        };
        if (i == selected_real) {
            SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
        } else if (b->is_new) {
            SDL_SetRenderDrawColor(renderer, 58, 195, 108, 255);
        } else if (b->is_moved) {
            SDL_SetRenderDrawColor(renderer, 180, 120, 230, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 74, 146, 206, 255);
        }
        SDL_RenderFillRect(renderer, &br);
        SDL_SetRenderDrawColor(renderer, 250, 250, 250, 255);
        SDL_RenderDrawRect(renderer, &br);
    }

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
        SDL_SetRenderDrawColor(renderer, valid_spot ? 80 : 240, valid_spot ? 240 : 70, valid_spot ? 100 : 70, 170);
        SDL_RenderFillRect(renderer, &ghost);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &ghost);
    }

    SDL_Rect map_border = { map_x - 1, map_y - 1, map_w + 2, map_h + 2 };
    SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
    SDL_RenderDrawRect(renderer, &map_border);

    const int rx = 454;
    int active_count = save_data_active_building_count(save);
    bool is_add_mode = (ui->bld_mode == BLD_MODE_ADD_SELECT || ui->bld_mode == BLD_MODE_ADD_PLACE);

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

    draw_text(renderer, ui->font_small,
              "Carte: Bleu=Bat.  Jaune=Coffre  Cyan=Arros.  Orange=Mach.  Vert=Arbre",
              rx, 448, col_muted);
}

/*
 * Rendu de l'onglet 5 : Inventaire Joueur / Coffres réels + Catalogue complet PS Vita 1.2
 */
static void render_items_tab(UIState *ui, SDL_Renderer *renderer, const SaveData *save) {
    SDL_Color col_gold  = { 248, 206, 102, 255 };
    SDL_Color col_text  = { 245, 236, 220, 255 };
    SDL_Color col_muted = { 185, 165, 145, 255 };
    SDL_Color col_green = { 120, 225, 120, 255 };
    SDL_Color col_cyan  = { 115, 215, 235, 255 };

    if (save->storage_count <= 0) {
        draw_text(renderer, ui->font_body, "Aucun stockage disponible.", 44, 180, col_muted);
        return;
    }

    int st_idx = ui->items_storage_idx;
    if (st_idx >= save->storage_count) {
        st_idx = 0;
    }
    const StorageContainer *st = &save->storages[st_idx];
    int used = save_data_storage_used_count(st);

    bool focus_left  = (ui->items_focus == ITEMS_FOCUS_STORAGE);
    bool focus_right = (ui->items_focus == ITEMS_FOCUS_CATALOG || ui->items_focus == ITEMS_FOCUS_QTY);

    /* 1. Barre sélecteur de zone de stockage (Inventaire Joueur ou Coffres) */
    SDL_Rect storage_bar = { 34, 148, 892, 32 };
    SDL_SetRenderDrawColor(renderer, focus_left ? 86 : 58, focus_left ? 58 : 42, focus_left ? 38 : 34, 255);
    SDL_RenderFillRect(renderer, &storage_bar);
    SDL_SetRenderDrawColor(renderer, focus_left ? 248 : 118, focus_left ? 206 : 82, focus_left ? 102 : 54, 255);
    SDL_RenderDrawRect(renderer, &storage_bar);

    char bar_txt[160];
    snprintf(bar_txt, sizeof(bar_txt), "Stockage (%d/%d) :   <  %s (%d/%d)  >",
             st_idx + 1, save->storage_count, st->label, used, st->max_slots);
    draw_text(renderer, ui->font_body, bar_txt, storage_bar.x + 14, storage_bar.y + 6,
              focus_left ? col_gold : col_text);
    draw_text(renderer, ui->font_small, "[Tab / Carre : changer panneau]",
              storage_bar.x + 650, storage_bar.y + 8, col_muted);

    /* 2. Panneau gauche : Contenu réel de la zone sélectionnée */
    SDL_Rect left_box = { 34, 188, 434, 282 };
    SDL_SetRenderDrawColor(renderer, 42, 30, 24, 255);
    SDL_RenderFillRect(renderer, &left_box);
    SDL_SetRenderDrawColor(renderer, focus_left ? 248 : 96, focus_left ? 206 : 68, focus_left ? 102 : 48, 255);
    SDL_RenderDrawRect(renderer, &left_box);

    char left_hdr[96];
    snprintf(left_hdr, sizeof(left_hdr), "Contenu (%d/%d) - Slot #%02d",
             used, st->max_slots, ui->items_slot_idx + 1);
    draw_text(renderer, ui->font_small, left_hdr, left_box.x + 12, left_box.y + 8,
              focus_left ? col_gold : col_muted);

    const int visible_slots = 7;
    int slot_start = 0;
    if (st->max_slots > visible_slots) {
        slot_start = clamp_int(ui->items_slot_idx - visible_slots / 2, 0, st->max_slots - visible_slots);
    }
    int show_slots = st->max_slots < visible_slots ? st->max_slots : visible_slots;
    for (int i = 0; i < show_slots; i++) {
        int sidx = slot_start + i;
        const StorageSlot *slot = &st->slots[sidx];
        SDL_Rect srow = { left_box.x + 10, left_box.y + 32 + i * 31, left_box.w - 20, 27 };
        bool sel = (focus_left && sidx == ui->items_slot_idx);

        if (sel) {
            SDL_SetRenderDrawColor(renderer, 96, 64, 40, 255);
            SDL_RenderFillRect(renderer, &srow);
            SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
            SDL_RenderDrawRect(renderer, &srow);
        } else {
            SDL_SetRenderDrawColor(renderer, 54, 39, 31, 255);
            SDL_RenderFillRect(renderer, &srow);
        }

        char slot_line[96];
        if (slot->occupied) {
            snprintf(slot_line, sizeof(slot_line), "%02d. %s%s",
                     sidx + 1, slot->name, slot->is_modified ? " *" : "");
        } else {
            snprintf(slot_line, sizeof(slot_line), "%02d. (Vide)", sidx + 1);
        }
        draw_text(renderer, ui->font_small, slot_line, srow.x + 10, srow.y + 5,
                  slot->occupied ? (sel ? col_gold : (slot->is_modified ? col_green : col_text)) : col_muted);

        if (slot->occupied) {
            char qbuf[24];
            snprintf(qbuf, sizeof(qbuf), "x%d", slot->stack);
            draw_text(renderer, ui->font_small, qbuf, srow.x + 340, srow.y + 5, col_cyan);
        }
    }

    draw_text(renderer, ui->font_small, "Gauche/Droite: Changer coffre  |  X: Retirer",
              left_box.x + 12, left_box.y + 256, col_muted);

    /* 3. Panneau droit : Catalogue officiel PS Vita (492 objets) */
    SDL_Rect right_box = { 480, 188, 446, 282 };
    SDL_SetRenderDrawColor(renderer, 42, 30, 24, 255);
    SDL_RenderFillRect(renderer, &right_box);
    SDL_SetRenderDrawColor(renderer, focus_right ? 248 : 96, focus_right ? 206 : 68, focus_right ? 102 : 48, 255);
    SDL_RenderDrawRect(renderer, &right_box);

    SDL_Rect cat_bar = { right_box.x + 10, right_box.y + 8, right_box.w - 20, 26 };
    SDL_SetRenderDrawColor(renderer, 66, 46, 34, 255);
    SDL_RenderFillRect(renderer, &cat_bar);

    int filtered = items_db_filtered_count(ui->items_category_idx);
    char cat_txt[96];
    snprintf(cat_txt, sizeof(cat_txt), "Categorie : < %s >  (%d objets)",
             ITEM_CAT_NAMES[ui->items_category_idx], filtered);
    draw_text(renderer, ui->font_small, cat_txt, cat_bar.x + 10, cat_bar.y + 5,
              focus_right ? col_gold : col_text);

    const int visible_cat = 6;
    int cat_start = 0;
    if (filtered > visible_cat) {
        cat_start = clamp_int(ui->items_catalog_cursor - visible_cat / 2, 0, filtered - visible_cat);
    }
    int show_cat = filtered < visible_cat ? filtered : visible_cat;
    for (int i = 0; i < show_cat; i++) {
        int nth = cat_start + i;
        int db_idx = items_db_nth(ui->items_category_idx, nth);
        const ItemDef *it = &ITEMS_DB[db_idx];
        SDL_Rect crow = { right_box.x + 10, right_box.y + 42 + i * 30, right_box.w - 20, 26 };
        bool sel = (focus_right && nth == ui->items_catalog_cursor);

        if (sel) {
            SDL_SetRenderDrawColor(renderer, 96, 64, 40, 255);
            SDL_RenderFillRect(renderer, &crow);
            SDL_SetRenderDrawColor(renderer, 248, 206, 102, 255);
            SDL_RenderDrawRect(renderer, &crow);
        }

        draw_text(renderer, ui->font_small, it->name, crow.x + 10, crow.y + 5, sel ? col_gold : col_text);

        char meta[48];
        snprintf(meta, sizeof(meta), "[%s] %dg", ITEM_CAT_NAMES[it->ui_category], it->price);
        draw_text(renderer, ui->font_small, meta, crow.x + 255, crow.y + 5, col_muted);
    }

    /* Barre de quantité / confirmation en bas du panneau droit */
    SDL_Rect qty_bar = { right_box.x + 10, right_box.y + 228, right_box.w - 20, 44 };
    if (ui->items_focus == ITEMS_FOCUS_QTY) {
        SDL_SetRenderDrawColor(renderer, 54, 92, 52, 255);
        SDL_RenderFillRect(renderer, &qty_bar);
        SDL_SetRenderDrawColor(renderer, 120, 225, 120, 255);
        SDL_RenderDrawRect(renderer, &qty_bar);

        char qline[128];
        snprintf(qline, sizeof(qline), "Quantite : <  x%d  >   (Haut/Bas: +/-10)", ui->items_add_qty);
        draw_text(renderer, ui->font_body, qline, qty_bar.x + 12, qty_bar.y + 4, col_gold);
        draw_text(renderer, ui->font_small, "Entree/Croix: Confirmer l'ajout  |  Backspace/Rond: Annuler",
                  qty_bar.x + 12, qty_bar.y + 24, col_green);
    } else {
        SDL_SetRenderDrawColor(renderer, 58, 42, 34, 255);
        SDL_RenderFillRect(renderer, &qty_bar);
        draw_text(renderer, ui->font_small, "Gauche/Droite: Categorie  |  Haut/Bas: Choisir objet",
                  qty_bar.x + 12, qty_bar.y + 5, col_muted);
        draw_text(renderer, ui->font_small, "Entree/Croix: Choisir quantite & Ajouter au stockage",
                  qty_bar.x + 12, qty_bar.y + 23, col_green);
    }
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

    /* 3. Barre de 6 onglets calés sur 960 px */
    int tab_w = 144;
    int tab_x = 23;
    for (int i = 0; i < TAB_COUNT; i++) {
        SDL_Rect tab_rect = { tab_x + i * (tab_w + 10), 84, tab_w, 42 };
        if (i == ui->active_tab) {
            SDL_SetRenderDrawColor(renderer, 166, 104, 46, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 58, 42, 34, 255);
        }
        SDL_RenderFillRect(renderer, &tab_rect);
        draw_text(renderer, ui->font_body, TAB_NAMES[i], tab_rect.x + 10, tab_rect.y + 11,
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
    } else if (ui->active_tab == 5) {
        render_items_tab(ui, renderer, save);
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

            draw_text(renderer, ui->font_body, label, row_rect.x + 20, row_rect.y + 13, col_text);

            char decorated_val[80];
            if (selected) {
                snprintf(decorated_val, sizeof(decorated_val), "<  %s  >", val_buf);
            } else {
                snprintf(decorated_val, sizeof(decorated_val), "%s", val_buf);
            }
            draw_text(renderer, ui->font_body, decorated_val, row_rect.x + 560, row_rect.y + 13,
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

    const char *help_text;
    if (ui->active_tab == 4) {
        help_text = "Fleches: Deplacer  |  Entree: Valider  |  Tab/Carre: Mode  |  X/Tri: Suppr  |  Q/E: Onglets  |  F5: Sauver";
    } else if (ui->active_tab == 5) {
        help_text = "Tab/Carre: Stockage/Catalogue  |  Gauche/Droite: Coffre/Cat  |  Entree: Ajouter  |  X: Retirer  |  F5: Sauver";
    } else {
        help_text = "Haut/Bas: Naviguer  |  Gauche/Droite: Modifier  |  Entree: Charger  |  Q/E (L/R): Onglets  |  F5: Sauver";
    }
    draw_text(renderer, ui->font_small, help_text, 24, SCREEN_HEIGHT - 27, col_muted);

    SDL_RenderPresent(renderer);
}
