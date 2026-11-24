/*
 * main.c — Point d'entrée de l'application Stardew Valley Save Editor.
 * Initialise les bibliothèques (libxml2, SDL2, SDL2_ttf), charge une sauvegarde,
 * exécute la boucle principale (~60 FPS) puis nettoie toutes les ressources.
 */

#include "core/save_data.h"
#include "platform/platform.h"
#include "ui/ui.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <libxml/parser.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * Fonction principale :
 * - `argc` / `argv` : permet optionnellement de passer un chemin de fichier XML en argument.
 * Retourne 0 en cas de fermeture normale, 1 en cas d'erreur d'initialisation.
 */
int main(int argc, char *argv[]) {
    /* 1. Initialisation du parser XML (libxml2) */
    xmlInitParser();

    /* 2. Initialisation de SDL2 (vidéo + gestion manette / boutons PS Vita) */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "Erreur SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    /* 3. Initialisation de SDL2_ttf pour l'affichage des polices */
    if (TTF_Init() != 0) {
        fprintf(stderr, "Erreur TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    /* 4. Création de la fenêtre à la résolution exacte de la PS Vita (960x544) */
    SDL_Window *window = SDL_CreateWindow(
        "Stardew Valley Save Editor (PS Vita 960x544)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Erreur SDL_CreateWindow: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    /* 5. Création du moteur de rendu 2D accéléré avec synchronisation verticale */
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        fprintf(stderr, "Erreur SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    /* 6. Ouverture de la première manette détectée (ou contrôles PS Vita sur console) */
    SDL_GameController *controller = NULL;
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        controller = SDL_GameControllerOpen(0);
    }

    /* 7. Initialisation de l'interface (chargement des polices) */
    UIState ui;
    if (!ui_init(&ui, platform_get_font_path())) {
        if (controller) SDL_GameControllerClose(controller);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    /*
     * 8. Chargement de la sauvegarde XML par ordre de priorité :
     *    a) Fichier passé en argument en ligne de commande
     *    b) Vraie sauvegarde PS Vita de test (EDWN_117938427)
     *    c) Petite sauvegarde d'exemple (sample_save.xml)
     *    d) Données de démonstration en mémoire
     */
    SaveData save;
    bool loaded = false;
    if (argc > 1) {
        loaded = save_data_load_xml(&save, argv[1]);
    }
    if (!loaded) {
        char vita_sample[512];
        snprintf(vita_sample, sizeof(vita_sample), "%s/vita_pcse01235/EDWN_117938427/EDWN_117938427",
                 platform_get_saves_dir());
        loaded = save_data_load_xml(&save, vita_sample);
    }
    if (!loaded) {
        char sample_path[512];
        snprintf(sample_path, sizeof(sample_path), "%s/sample_save.xml", platform_get_saves_dir());
        loaded = save_data_load_xml(&save, sample_path);
    }
    if (!loaded) {
        save_data_init_demo(&save);
    }

    /* 9. Boucle principale : lecture des entrées, mise à jour et rendu (~60 FPS) */
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            InputAction action = platform_translate_event(&event);
            if (action == INPUT_QUIT) {
                running = false;
            } else if (action != INPUT_NONE) {
                ui_handle_input(&ui, &save, action);
            }
        }

        ui_render(&ui, renderer, &save);
        SDL_Delay(16);
    }

    /* 10. Libération propre de toutes les ressources avant de quitter */
    ui_destroy(&ui);
    if (controller) SDL_GameControllerClose(controller);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    xmlCleanupParser();
    return 0;
}
