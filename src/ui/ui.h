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

/*
 * Contient tout l'état interne de l'interface :
 * polices chargées, onglet actif, ligne sélectionnée et message temporaire.
 */
typedef struct {
    TTF_Font *font_title;   /* Police grande taille pour le titre (26 px) */
    TTF_Font *font_body;    /* Police standard pour onglets et lignes (20 px) */
    TTF_Font *font_small;   /* Police petite taille pour sous-titre et barre d'aide (15 px) */

    int active_tab;         /* Index de l'onglet actif (0: Joueur, 1: Compétences, 2: Ferme) */
    int selected_row;       /* Index de la ligne actuellement sélectionnée dans l'onglet */

    char status_msg[128];   /* Message de confirmation/erreur affiché en bas de l'écran */
    Uint32 status_until_ms; /* Timestamp SDL (en ms) jusqu'auquel afficher status_msg */
} UIState;

/* Charge les 3 tailles de police depuis `font_path` et initialise `ui`. Retourne true si succès. */
bool ui_init(UIState *ui, const char *font_path);

/* Libère les polices chargées par `ui_init`. */
void ui_destroy(UIState *ui);

/* Traite une action utilisateur (navigation, changement d'onglet, édition, sauvegarde). */
void ui_handle_input(UIState *ui, SaveData *save, InputAction action);

/* Dessine l'ensemble de l'interface dans le renderer SDL (résolution 960x544). */
void ui_render(UIState *ui, SDL_Renderer *renderer, const SaveData *save);

#endif
