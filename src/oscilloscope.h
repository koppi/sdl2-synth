#pragma once

#include <SDL_ttf.h>
#include "synth.h"
#include <SDL.h>
#define FFT_SIZE 2048
#define WATERFALL_HEIGHT 256

void oscilloscope_init();
void oscilloscope_shutdown();
void oscilloscope_feed(float sample);
void oscilloscope_update_fft(float sample);
void oscilloscope_draw(SDL_Renderer *renderer, const struct Synth *synth, int x, int y,
                       int w, int h, TTF_Font *font);
