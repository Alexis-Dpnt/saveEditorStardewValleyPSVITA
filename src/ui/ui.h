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

/* Sous-mode actif dans l'onglet Batiments */
typedef enum {
    BLD_MODE_SELECT = 0, /* Choix du batiment dans le catalogue */
    BLD_MODE_PLACE  = 1  /* Deplacement et pose du batiment sur la mini-carte */
} BuildingMode;

/*
 * Contient tout l'état interne de l'interface :
 * polices chargées, onglet actif, ligne sélectionnée, état du constructeur
 * de bâtiments et message temporaire.
 */
typedef struct {
    TTF_Font *font_title;   /* Police grande taille pour le titre (26 px) */
    TTF_Font *font_body;    /* Police standard pour onglets et lignes (20 px) */
    TTF_Font *font_small;   /* Police petite taille pour sous-titre et barre d'aide (15 px) */

    int active_tab;         /* Index de l'onglet actif (0: Joueur, 1: Compétences, 2: Ferme, 3: Bâtiments) */
    int selected_row;       /* Index de la ligne actuellement sélectionnée dans l'onglet */

    /* État spécifique à l'onglet Bâtiments (tab == 3) */
    BuildingMode bld_mode;  /* SELECT (catalogue) ou PLACE (curseur sur la carte) */
    int bld_catalog_idx;    /* Index du bâtiment choisi dans BUILDING_CATALOG */
    int cursor_x;           /* Position X du curseur de placement sur la ferme (0..79) */
    int cursor_y;           /* Position Y du curseur de placement sur la ferme (0..64) */

    char status_msg[128];   /* Message de confirmation/erreur affiché en bas de l'écran */
    Uint32 status_until_ms; /* Timestamp SDL (en ms) jusqu'auquel afficher status_msg */
} UIState;

/* Charge les 3 tailles de police depuis `font_path` et initialise `ui`. Retourne true si succès. */
bool ui_init(UIState *ui, const char *font_path);

/* Libère les polices chargées par `ui_init`. */
void ui_destroy(UIState *ui);

/* Traite une action utilisateur (navigation, changement d'onglet, édition, pose bâtiment, sauvegarde). */
void ui_handle_input(UIState *ui, SaveData *save, InputAction action);

/* Dessine l'ensemble de l'interface dans le renderer SDL (résolution 960x544). */
void ui_render(UIState *ui, SDL_Renderer *renderer, const SaveData *save);

#endif
