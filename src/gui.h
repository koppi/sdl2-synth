#pragma once
#include "synth.h"
#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

void gui_init(SDL_Window *window, SDL_GLContext gl_context);
void gui_shutdown();
void gui_handle_event(const SDL_Event *event);
void gui_set_humanize_vars(int *enabled, float *vel_var, float *time_var, float *bpm, void (*toggle_func)(void), Synth *synth);
void gui_draw(Synth *synth, SDL_Window *window, SDL_GLContext gl_context, 
              int chord_prog_enabled, int current_chord_idx, int chord_prog_len,
              int current_rhythm_step, int rhythm_len);
void gui_set_key_pressed(int midi_note, int is_pressed);

#ifdef __cplusplus
}
#endif
