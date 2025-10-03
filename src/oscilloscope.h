#pragma once

#include "synth.h"
#include <SDL.h>
#include <SDL_ttf.h>

#define FFT_SIZE 1024
#define WATERFALL_HEIGHT 128

#define FFT_SIZE 1024
#define WATERFALL_HEIGHT 128

void oscilloscope_init();
void oscilloscope_shutdown();
void oscilloscope_feed(float sample);
void oscilloscope_update_fft(float sample);
void oscilloscope_draw(SDL_Renderer *renderer, const struct Synth *synth, int x, int y,
                       int w, int h, TTF_Font *font);
