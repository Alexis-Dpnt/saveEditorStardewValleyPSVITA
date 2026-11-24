#include "ui/ui.h"

#include <stdio.h>
#include <string.h>

#define TAB_COUNT 3

static const char *TAB_NAMES[TAB_COUNT] = {
    "Joueur",
    "Competences",
    "Ferme & Temps"
};

static int tab_row_count(int tab) {
    switch (tab) {
        case 0: return 3; /* Argent, Energie max, Sante max */
        case 1: return 5; /* Agriculture, Minage, Cueillette, Peche, Combat */
        case 2: return 3; /* Jour, Saison, Annee */
        default: return 0;
    }
}

static int clamp_int(int v, int min_v, int max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static void set_status(UIState *ui, const char *msg) {
    snprintf(ui->status_msg, sizeof(ui->status_msg), "%s", msg);
    ui->status_until_ms = SDL_GetTicks() + 3000;
}

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

bool ui_init(UIState *ui, const char *font_path) {
    memset(ui, 0, sizeof(*ui));

    ui->font_title = TTF_OpenFont(font_path, 26);
    ui->font_body  = TTF_OpenFont(font_path, 20);
    ui->font_small = TTF_OpenFont(font_path, 15);

    if (!ui->font_title || !ui->font_body || !ui->font_small) {
        fprintf(stderr, "Erreur chargement police (%s): %s\n", font_path, TTF_GetError());
        return false;
    }
    return true;
}

void ui_destroy(UIState *ui) {
    if (ui->font_title) TTF_CloseFont(ui->font_title);
    if (ui->font_body)  TTF_CloseFont(ui->font_body);
    if (ui->font_small) TTF_CloseFont(ui->font_small);
}

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

void ui_handle_input(UIState *ui, SaveData *save, InputAction action) {
    int rows = tab_row_count(ui->active_tab);

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
        case INPUT_TAB_PREV:
            ui->active_tab = (ui->active_tab - 1 + TAB_COUNT) % TAB_COUNT;
            ui->selected_row = 0;
            break;
        case INPUT_TAB_NEXT:
            ui->active_tab = (ui->active_tab + 1) % TAB_COUNT;
            ui->selected_row = 0;
            break;
        case INPUT_SAVE:
            if (save_data_save_xml(save)) {
                set_status(ui, "Sauvegarde enregistree (+ backup .bak cree) !");
            } else {
                set_status(ui, "Mode demo : aucun fichier XML charge a sauvegarder.");
            }
            break;
        default:
            break;
    }
}

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

void ui_render(UIState *ui, SDL_Renderer *renderer, const SaveData *save) {
    /* Fond bois sombre style Stardew */
    SDL_SetRenderDrawColor(renderer, 36, 26, 22, 255);
    SDL_RenderClear(renderer);

    /* Bandeau header */
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

    /* Onglets */
    int tab_w = 220;
    int tab_x = 24;
    for (int i = 0; i < TAB_COUNT; i++) {
        SDL_Rect tab_rect = { tab_x + i * (tab_w + 12), 84, tab_w, 42 };
        if (i == ui->active_tab) {
            SDL_SetRenderDrawColor(renderer, 166, 104, 46, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 58, 42, 34, 255);
        }
        SDL_RenderFillRect(renderer, &tab_rect);
        draw_text(renderer, ui->font_body, TAB_NAMES[i], tab_rect.x + 18, tab_rect.y + 9,
                  i == ui->active_tab ? col_gold : col_muted);
    }

    /* Panneau principal */
    SDL_Rect panel = { 24, 140, SCREEN_WIDTH - 48, 336 };
    SDL_SetRenderDrawColor(renderer, 50, 36, 28, 255);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 118, 82, 54, 255);
    SDL_RenderDrawRect(renderer, &panel);

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

    /* Message de statut */
    if (ui->status_msg[0] != '\0' && SDL_GetTicks() < ui->status_until_ms) {
        draw_text(renderer, ui->font_small, ui->status_msg, 24, 484, col_accent);
    }

    /* Bandeau footer */
    SDL_Rect footer = { 0, SCREEN_HEIGHT - 38, SCREEN_WIDTH, 38 };
    SDL_SetRenderDrawColor(renderer, 26, 18, 15, 255);
    SDL_RenderFillRect(renderer, &footer);

    draw_text(renderer, ui->font_small,
              "Haut/Bas: Naviguer  |  Gauche/Droite: Modifier  |  Q/E (L/R): Onglets  |  F5 (Start): Sauvegarder  |  Esc: Quitter",
              24, SCREEN_HEIGHT - 28, col_muted);

    SDL_RenderPresent(renderer);
}
