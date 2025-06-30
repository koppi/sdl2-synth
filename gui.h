#ifndef GUI_H
#define GUI_H
#include <SDL2/SDL.h>
#include "synth.h"

void gui_init(SDL_Renderer *ren);
void gui_quit();
void gui_draw_keyboard(SDL_Renderer *ren, Synth *s);

#endif