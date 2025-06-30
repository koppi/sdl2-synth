#include "osc.h"
#include <math.h>

float osc_sine(float phase) {
    return sinf(phase * 2.0f * (float)M_PI);
}

float osc_square(float phase) {
    return (phase < 0.5f ? 1.0f : -1.0f);
}