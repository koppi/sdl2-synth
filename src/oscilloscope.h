#pragma once
#include "synth.h"
#include <SDL.h>
#include <SDL_ttf.h>

void oscilloscope_draw(SDL_Renderer *renderer, const Synth *synth, int x, int y, int w, int h, TTF_Font *font);
