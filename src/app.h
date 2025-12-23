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
  
  // Chord progression
  int chord_progression_enabled;
  int current_chord_index;
  float chord_progression_timer;
  float chord_duration;
  int chord_progression[8];
  int chord_progression_length;
  float rhythm_pattern[128];
  int rhythm_pattern_length;
  int current_rhythm_step;
  float rhythm_step_timer;
  int active_chord_notes[16];
  int active_chord_note_count;
  float humanize_velocity_amount;
  float humanize_timing_amount;
  float bpm;
} App;

int app_init(App *app);
void app_poll_events(App *app);
void app_update(App *app);
void app_render(App *app);
void app_shutdown(App *app);