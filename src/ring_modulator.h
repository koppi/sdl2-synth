#pragma once

typedef struct RingModulator {
    float frequency;
    float mix;        // 0.0 = dry, 1.0 = fully ring modulated
    float phase;
    float sample_rate;
    int enabled;
} RingModulator;

void ring_mod_init(RingModulator *rm, float sample_rate);
void ring_mod_set_frequency(RingModulator *rm, float frequency);
void ring_mod_set_mix(RingModulator *rm, float mix);
void ring_mod_set_enabled(RingModulator *rm, int enabled);
void ring_mod_process(RingModulator *rm, float *input, int frames);