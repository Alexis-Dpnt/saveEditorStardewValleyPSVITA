/*
 * platform.c — Implémentation des fonctions spécifiques à la plateforme (macOS / PS Vita).
 */

#include "platform/platform.h"

#include <stdio.h>

/*
 * Vérifie si un fichier existe et peut être ouvert en lecture.
 * Utilisé sur macOS pour choisir une police disponible.
 */
static bool file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

/*
 * Retourne le chemin du dossier des sauvegardes :
 * - Sur PS Vita : dossier dédié sur la carte mémoire (ux0:data/...)
 * - Sur macOS   : dossier local test_saves/ à la racine du projet
 */
const char *platform_get_saves_dir(void) {
#ifdef PLATFORM_VITA
    return "ux0:data/StardewSaveEditor/saves";
#else
    return "test_saves";
#endif
}

/*
 * Retourne le chemin de la police TrueType utilisée par SDL2_ttf :
 * - Sur PS Vita : police embarquée dans le .vpk (app0:assets/font.ttf)
 * - Sur macOS   : assets/font.ttf si présente, sinon Arial ou Helvetica système
 */
const char *platform_get_font_path(void) {
#ifdef PLATFORM_VITA
    return "app0:assets/font.ttf";
#else
    if (file_exists("assets/font.ttf")) {
        return "assets/font.ttf";
    }
    if (file_exists("/System/Library/Fonts/Supplemental/Arial.ttf")) {
        return "/System/Library/Fonts/Supplemental/Arial.ttf";
    }
    return "/System/Library/Fonts/Helvetica.ttc";
#endif
}

/*
 * Convertit un événement SDL (fermeture fenêtre, touche clavier ou bouton manette/Vita)
 * en une action abstraite InputAction comprise par l'interface.
 */
InputAction platform_translate_event(const SDL_Event *event) {
    /* Fermeture de la fenêtre (clic sur la croix rouge sur macOS) */
    if (event->type == SDL_QUIT) {
        return INPUT_QUIT;
    }

    /* Entrées clavier (développement sur macOS) */
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_UP:
            case SDLK_w:
                return INPUT_UP;
            case SDLK_DOWN:
            case SDLK_s:
                return INPUT_DOWN;
            case SDLK_LEFT:
            case SDLK_a:
                return INPUT_LEFT;
            case SDLK_RIGHT:
            case SDLK_d:
                return INPUT_RIGHT;
            case SDLK_RETURN:
            case SDLK_SPACE:
                return INPUT_CONFIRM;
            case SDLK_BACKSPACE:
                return INPUT_CANCEL;
            case SDLK_q:
            case SDLK_LEFTBRACKET:
                return INPUT_TAB_PREV;
            case SDLK_e:
            case SDLK_RIGHTBRACKET:
                return INPUT_TAB_NEXT;
            case SDLK_F5:
                return INPUT_SAVE;
            case SDLK_ESCAPE:
                return INPUT_QUIT;
            default:
                break;
        }
    }

    /* Entrées manette / boutons physiques de la PS Vita */
    if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                return INPUT_UP;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                return INPUT_DOWN;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                return INPUT_LEFT;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                return INPUT_RIGHT;
            case SDL_CONTROLLER_BUTTON_A:
                return INPUT_CONFIRM;
            case SDL_CONTROLLER_BUTTON_B:
                return INPUT_CANCEL;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                return INPUT_TAB_PREV;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                return INPUT_TAB_NEXT;
            case SDL_CONTROLLER_BUTTON_START:
                return INPUT_SAVE;
            case SDL_CONTROLLER_BUTTON_BACK:
                return INPUT_QUIT;
            default:
                break;
        }
    }

    return INPUT_NONE;
}
