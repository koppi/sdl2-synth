#ifndef OSC_H
#define OSC_H

#define OSC_SINE    1
#define OSC_SQUARE  2
#define OSC_NOISE   4

float osc_sine(float phase);
float osc_square(float phase);
float osc_noise();

#endif