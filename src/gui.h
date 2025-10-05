#pragma once
#include "synth.h"
#include <SDL.h>
#include <SDL_ttf.h>

typedef struct Gui {
  SDL_Renderer *renderer;
  Synth *synth;
  int selected_control;
} Gui;

void gui_init(Gui *gui, SDL_Renderer *renderer, Synth *synth);
void gui_shutdown(Gui *gui);
void gui_handle_event(Gui *gui, const SDL_Event *e);
void gui_update(Gui *gui);
void gui_draw(Gui *gui);
TTF_Font* gui_get_font();
void gui_set_key_pressed(int midi_note, int is_pressed);