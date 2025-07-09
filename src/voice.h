#pragma once
#include "osc.h"

typedef struct Voice {
    int active;
    float note;
    float velocity;
    float env_phase;
    unsigned long long timestamp; // For voice stealing: tracks when voice was last activated
} Voice;

void voice_init(Voice *v, float samplerate);
void voice_on(Voice *v, float note, float velocity, unsigned long long timestamp);
void voice_off(Voice *v);
void voice_render(Voice *v, const Oscillator *osc, float *stereo, int frames);