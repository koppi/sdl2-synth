#pragma once
#include "gui.h"
#include "synth.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_AudioDeviceID audio;
  Synth synth;
  Gui gui;
  int quit;
  int show_help;
  TTF_Font *font;
  
  // Frame rate timing
  Uint32 last_frame_time;
  Uint32 frame_count;
  float fps;
  Uint32 target_frame_time; // 1000ms / 60fps = ~16.67ms
} App;

int app_init(App *app);
void app_poll_events(App *app);
void app_update(App *app);
void app_render(App *app);
void app_shutdown(App *app);
