/*
 * ui.h — État et fonctions de l'interface graphique maison (SDL2 + SDL2_ttf).
 */

#ifndef UI_H
#define UI_H

#include "core/save_data.h"
#include "platform/platform.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>

/* Sous-mode actif dans l'onglet Bâtiments */
typedef enum {
    BLD_MODE_ADD_SELECT  = 0, /* Choisir un nouveau bâtiment dans le catalogue */
    BLD_MODE_ADD_PLACE   = 1, /* Placer le nouveau bâtiment sur la mini-carte */
    BLD_MODE_MANAGE_LIST = 2, /* Parcourir les bâtiments existants (déplacer / supprimer) */
    BLD_MODE_MANAGE_MOVE = 3  /* Déplacer le bâtiment existant sélectionné sur la carte */
} BuildingMode;

/*
 * Contient tout l'état interne de l'interface.
 */
typedef struct {
    TTF_Font *font_title;   /* Police titre (26 px) */
    TTF_Font *font_body;    /* Police standard (18 px) */
    TTF_Font *font_small;   /* Police petite (14 px) */

    int active_tab;         /* 0: Sauvegardes, 1: Joueur, 2: Compétences, 3: Ferme, 4: Bâtiments */
    int selected_row;       /* Ligne sélectionnée dans l'onglet actif */

    /* Sélecteur de sauvegardes (tab == 0) */
    SaveFileEntry save_files[MAX_SAVE_FILES];
    int save_file_count;

    /* Constructeur / gestionnaire de bâtiments (tab == 4) */
    BuildingMode bld_mode;
    int bld_catalog_idx;    /* Index dans BUILDING_CATALOG (0..15) */
    int bld_existing_idx;   /* Index dans save->buildings[] pour déplacer/supprimer */
    int cursor_x;           /* Curseur X sur la ferme (0..79) */
    int cursor_y;           /* Curseur Y sur la ferme (0..64) */

    char status_msg[128];   /* Message temporaire en bas de l'écran */
    Uint32 status_until_ms; /* Fin d'affichage de status_msg */
} UIState;

/* Charge les polices, scanne les sauvegardes et initialise `ui`. */
bool ui_init(UIState *ui, const char *font_path);

/* Libère les polices chargées par `ui_init`. */
void ui_destroy(UIState *ui);

/* Traite une action utilisateur pour tous les onglets. */
void ui_handle_input(UIState *ui, SaveData *save, InputAction action);

/* Dessine l'ensemble de l'interface en 960x544. */
void ui_render(UIState *ui, SDL_Renderer *renderer, const SaveData *save);

#endif
