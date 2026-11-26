/*
 * platform.c — Implémentation des fonctions spécifiques à la plateforme (macOS / PS Vita)
 * et scanner automatique des sauvegardes Stardew Valley (opendir / readdir).
 */

#include "platform/platform.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Vérifie si un fichier existe et peut être ouvert en lecture.
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
 * Vérifie si `path` est un dossier.
 */
static bool is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

/*
 * Retourne true si le nom se termine par `suffix`.
 */
static bool ends_with(const char *str, const char *suffix) {
    size_t slen = strlen(str);
    size_t suflen = strlen(suffix);
    if (slen < suflen) {
        return false;
    }
    return strcmp(str + slen - suflen, suffix) == 0;
}

/*
 * Retourne le chemin du dossier des sauvegardes :
 * - Sur PS Vita : dossier dédié sur la carte mémoire (ux0:data/StardewSaveEditor/saves)
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
 * Retourne le chemin de la police TrueType utilisée par SDL2_ttf.
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
 * Parcours récursif (profondeur limitée) d'un dossier pour trouver :
 * 1. Les dossiers de sauvegarde Stardew Valley (contenant SaveGameInfo + un fichier du même nom)
 * 2. Les fichiers `.xml` isolés (comme sample_save.xml)
 */
static void scan_dir_recursive(const char *dir_path, int depth,
                               SaveFileEntry *out_list, int max_count, int *count) {
    if (depth > 4 || *count >= max_count) {
        return;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && *count < max_count) {
        const char *name = ent->d_name;
        if (name[0] == '.') {
            continue;
        }
        /* Ignore les gros dossiers d'assets ou système qui ne contiennent pas de sauvegardes */
        if (strcmp(name, "app") == 0 || strcmp(name, "patch") == 0 ||
            strcmp(name, "sce_sys") == 0 || strcmp(name, "maps") == 0 ||
            strcmp(name, "extracted") == 0 || strcmp(name, "Content_extracted") == 0) {
            continue;
        }

        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir_path, name);

        if (is_directory(full)) {
            /* Vérifie si c'est un dossier de sauvegarde Stardew Valley (ex: Alexis_455215412/) */
            char main_file[512];
            char info_file[512];
            snprintf(main_file, sizeof(main_file), "%s/%s", full, name);
            snprintf(info_file, sizeof(info_file), "%s/SaveGameInfo", full);

            if (file_exists(main_file) && file_exists(info_file)) {
                snprintf(out_list[*count].label, sizeof(out_list[*count].label), "%s", name);
                snprintf(out_list[*count].path, sizeof(out_list[*count].path), "%s", main_file);
                (*count)++;
            } else {
                scan_dir_recursive(full, depth + 1, out_list, max_count, count);
            }
        } else if (depth == 0 && ends_with(name, ".xml")) {
            snprintf(out_list[*count].label, sizeof(out_list[*count].label), "%s", name);
            snprintf(out_list[*count].path, sizeof(out_list[*count].path), "%s", full);
            (*count)++;
        }
    }

    closedir(dir);
}

/*
 * Scanne le dossier des sauvegardes de la plateforme et remplit `out_list`.
 */
int platform_scan_saves(SaveFileEntry *out_list, int max_count) {
    int count = 0;
    scan_dir_recursive(platform_get_saves_dir(), 0, out_list, max_count, &count);
    return count;
}

/*
 * Convertit un événement SDL (fermeture fenêtre, clavier ou manette/PS Vita)
 * en une action abstraite InputAction.
 */
InputAction platform_translate_event(const SDL_Event *event) {
    if (event->type == SDL_QUIT) {
        return INPUT_QUIT;
    }

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
            case SDLK_x:
            case SDLK_DELETE:
                return INPUT_DELETE;
            case SDLK_TAB:
                return INPUT_MODE_SWITCH;
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
            case SDL_CONTROLLER_BUTTON_Y:
                return INPUT_DELETE;
            case SDL_CONTROLLER_BUTTON_X:
                return INPUT_MODE_SWITCH;
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
