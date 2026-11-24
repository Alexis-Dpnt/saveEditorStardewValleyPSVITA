#ifndef PLATFORM_H
#define PLATFORM_H

#include <SDL.h>
#include <stdbool.h>

#define SCREEN_WIDTH  960
#define SCREEN_HEIGHT 544

typedef enum {
    INPUT_NONE = 0,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_CONFIRM,
    INPUT_CANCEL,
    INPUT_TAB_PREV,
    INPUT_TAB_NEXT,
    INPUT_SAVE,
    INPUT_QUIT
} InputAction;

const char *platform_get_saves_dir(void);
const char *platform_get_font_path(void);
InputAction platform_translate_event(const SDL_Event *event);

#endif
