/*
 * platform.h — Couche d'abstraction entre macOS (bureau) et PS Vita.
 * Définit la résolution de l'écran, les actions utilisateur abstraites et
 * les fonctions dépendantes de la plateforme (chemins, entrées).
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <SDL.h>
#include <stdbool.h>

/* Résolution native de l'écran de la PS Vita (utilisée aussi sur macOS) */
#define SCREEN_WIDTH  960
#define SCREEN_HEIGHT 544

/*
 * Actions abstraites indépendantes du périphérique.
 * Permet d'utiliser le même code d'interface avec le clavier sur Mac
 * et avec les boutons / D-Pad sur PS Vita.
 */
typedef enum {
    INPUT_NONE = 0,  /* Aucun événement utile */
    INPUT_UP,        /* Déplacer la sélection vers le haut */
    INPUT_DOWN,      /* Déplacer la sélection vers le bas */
    INPUT_LEFT,      /* Diminuer la valeur sélectionnée */
    INPUT_RIGHT,     /* Augmenter la valeur sélectionnée */
    INPUT_CONFIRM,   /* Valider (Entrée / Espace / Croix) */
    INPUT_CANCEL,    /* Annuler / retour (Backspace / Rond) */
    INPUT_TAB_PREV,  /* Passer à l'onglet précédent (Q / L) */
    INPUT_TAB_NEXT,  /* Passer à l'onglet suivant (E / R) */
    INPUT_SAVE,      /* Enregistrer la sauvegarde (F5 / Start) */
    INPUT_QUIT       /* Quitter l'application (Esc / Select / fermeture fenêtre) */
} InputAction;

/* Retourne le dossier contenant les sauvegardes selon la plateforme (Mac ou Vita). */
const char *platform_get_saves_dir(void);

/* Retourne le chemin vers la police TrueType (.ttf/.ttc) à charger pour l'UI. */
const char *platform_get_font_path(void);

/* Traduit un événement brut SDL (clavier, manette, fenêtre) en InputAction. */
InputAction platform_translate_event(const SDL_Event *event);

#endif
