#include "osc.h"
#include <math.h>
#include <stdlib.h>

float osc_sine(float phase) {
    return sinf(phase * 2.0f * (float)M_PI);
}

float osc_square(float phase) {
    return (phase < 0.5f ? 1.0f : -1.0f);
}

float osc_noise() {
    // Returns float in [-1, 1]
    return 2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f;
}

float osc_saw(float phase) {
    // Sawtooth rises from -1 to 1 as phase goes 0..1
    return 2.0f * phase - 1.0f;
}