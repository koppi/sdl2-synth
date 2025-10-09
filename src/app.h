#pragma once
#include "synth.h"
#include <SDL2/SDL.h>

typedef struct {
  SDL_Window *window;
  SDL_GLContext gl_context;
  SDL_AudioDeviceID audio;
  Synth synth;
  int quit;
  int show_help;

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