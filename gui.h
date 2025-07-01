#ifndef GUI_H
#define GUI_H
#include <SDL2/SDL.h>
#include "synth.h"

void gui_init(SDL_Renderer *ren);
void gui_draw(SDL_Renderer *ren, Synth *synth, int knob_active, int knob_type, int knob_idx, int key_states[128]);
void gui_quit();

// Returns 1 if it handled a dropdown click
int gui_handle_waveform_dropdown(SDL_Event *e, Synth *synth);

extern const char* waveform_names[NUM_WAVEFORMS];

#define KNOB_RADIUS 32
#define OSC_CARD_W 140
#define OSC_CARD_H 210
#define NUM_EFFECT_KNOBS 4 // Flanger, Delay, Reverb, Volume

#endif