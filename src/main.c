/*
 * main.c — Point d'entrée de l'application Stardew Valley Save Editor.
 * Initialise libxml2, SDL2, SDL2_ttf, charge la première sauvegarde détectée
 * (ou celle passée en argument), exécute la boucle ~60 FPS et libère tout proprement.
 */

#include "core/save_data.h"
#include "platform/platform.h"
#include "ui/ui.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <libxml/parser.h>
#include <stdbool.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    xmlInitParser();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "Erreur SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() != 0) {
        fprintf(stderr, "Erreur TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

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

    SDL_GameController *controller = NULL;
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        controller = SDL_GameControllerOpen(0);
    }

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
     * Chargement initial :
     * 1. Fichier passé en argument (argv[1])
     * 2. Première sauvegarde trouvée par le scanner automatique (ui.save_files[0])
     * 3. Sinon mode démo en mémoire
     */
    SaveData save;
    bool loaded = false;
    if (argc > 1) {
        loaded = save_data_load_xml(&save, argv[1]);
    }
    if (!loaded && ui.save_file_count > 0) {
        loaded = save_data_load_xml(&save, ui.save_files[0].path);
    }
    if (!loaded) {
        save_data_init_demo(&save);
        ui.active_tab = 0; /* Ouvre directement l'onglet Sauvegardes s'il n'y a rien de chargé */
    }

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

    ui_destroy(&ui);
    if (controller) SDL_GameControllerClose(controller);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    xmlCleanupParser();
    return 0;
}
