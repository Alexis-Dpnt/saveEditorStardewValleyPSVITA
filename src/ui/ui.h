#ifndef UI_H
#define UI_H

#include "core/save_data.h"
#include "platform/platform.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>

typedef struct {
    TTF_Font *font_title;
    TTF_Font *font_body;
    TTF_Font *font_small;

    int active_tab;
    int selected_row;

    char status_msg[128];
    Uint32 status_until_ms;
} UIState;

bool ui_init(UIState *ui, const char *font_path);
void ui_destroy(UIState *ui);
void ui_handle_input(UIState *ui, SaveData *save, InputAction action);
void ui_render(UIState *ui, SDL_Renderer *renderer, const SaveData *save);

#endif
