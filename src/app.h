#pragma once
#include "synth.h"
#include "gui.h"
#include <SDL.h>

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioDeviceID audio;
    Synth synth;
    Gui gui;
    int quit;
} App;

int  app_init(App *app);
void app_poll_events(App *app);
void app_update(App *app);
void app_render(App *app);
void app_shutdown(App *app);