#include "ring_modulator.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void ring_mod_init(RingModulator *rm, float sample_rate) {
    rm->frequency = 100.0f;  // 100 Hz default
    rm->mix = 0.0f;          // Dry by default
    rm->phase = 0.0f;
    rm->sample_rate = sample_rate;
    rm->enabled = 0;         // Disabled by default
}

void ring_mod_set_frequency(RingModulator *rm, float frequency) {
    rm->frequency = frequency;
}

void ring_mod_set_mix(RingModulator *rm, float mix) {
    rm->mix = fmaxf(0.0f, fminf(1.0f, mix));
}

void ring_mod_set_enabled(RingModulator *rm, int enabled) {
    rm->enabled = enabled;
}

void ring_mod_process(RingModulator *rm, float *input, int frames) {
    if (!rm->enabled || rm->mix == 0.0f) {
        return;
    }
    
    for (int n = 0; n < frames; ++n) {
        // Generate carrier sine wave
        float carrier = sinf(2.0f * M_PI * rm->phase);
        
        // Ring modulation: multiply input signal with carrier
        float ring_modulated = input[n * 2 + 0] * carrier;
        float ring_modulated_right = input[n * 2 + 1] * carrier;
        
        // Mix dry and wet signals
        input[n * 2 + 0] = input[n * 2 + 0] * (1.0f - rm->mix) + ring_modulated * rm->mix;
        input[n * 2 + 1] = input[n * 2 + 1] * (1.0f - rm->mix) + ring_modulated_right * rm->mix;
        
        // Update phase for carrier oscillator
        rm->phase += rm->frequency / rm->sample_rate;
        if (rm->phase >= 1.0f) {
            rm->phase -= 1.0f;
        }
    }
}