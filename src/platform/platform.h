/*
 * platform.h — Couche d'abstraction entre macOS (bureau) et PS Vita.
 * Définit la résolution de l'écran, les actions utilisateur abstraites,
 * la détection automatique des sauvegardes et les chemins plateforme.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <SDL.h>
#include <stdbool.h>

/* Résolution native de l'écran de la PS Vita (utilisée aussi sur macOS) */
#define SCREEN_WIDTH  960
#define SCREEN_HEIGHT 544

#define MAX_SAVE_FILES 32

/* Entrée trouvée par le scanner de sauvegardes */
typedef struct {
    char label[96];  /* Nom lisible affiché dans l'onglet Sauvegardes */
    char path[512];  /* Chemin complet vers le fichier XML principal */
} SaveFileEntry;

/*
 * Actions abstraites indépendantes du périphérique.
 */
typedef enum {
    INPUT_NONE = 0,     /* Aucun événement utile */
    INPUT_UP,           /* Haut */
    INPUT_DOWN,         /* Bas */
    INPUT_LEFT,         /* Gauche */
    INPUT_RIGHT,        /* Droite */
    INPUT_CONFIRM,      /* Valider / Poser / Charger (Entrée / Espace / Croix) */
    INPUT_CANCEL,       /* Annuler / Retour (Backspace / Rond) */
    INPUT_DELETE,       /* Supprimer un bâtiment (X / Suppr / Triangle) */
    INPUT_MODE_SWITCH,  /* Basculer Ajouter <-> Gérer existants (Tab / Carré) */
    INPUT_TAB_PREV,     /* Onglet précédent (Q / L) */
    INPUT_TAB_NEXT,     /* Onglet suivant (E / R) */
    INPUT_SAVE,         /* Enregistrer la sauvegarde (F5 / Start) */
    INPUT_QUIT          /* Quitter l'application (Esc / Select / fermeture fenêtre) */
} InputAction;

/* Retourne le dossier contenant les sauvegardes selon la plateforme (Mac ou Vita). */
const char *platform_get_saves_dir(void);

/* Retourne le chemin vers la police TrueType (.ttf/.ttc) à charger pour l'UI. */
const char *platform_get_font_path(void);

/* Scanne le dossier des sauvegardes et remplit `out_list`. Retourne le nombre trouvé. */
int platform_scan_saves(SaveFileEntry *out_list, int max_count);

/* Traduit un événement brut SDL (clavier, manette, fenêtre) en InputAction. */
InputAction platform_translate_event(const SDL_Event *event);

#endif
