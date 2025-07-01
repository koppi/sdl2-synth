#ifndef OSC_H
#define OSC_H

#define OSC_SINE    1
#define OSC_SQUARE  2
#define OSC_NOISE   4
#define OSC_SAW     8   // <-- add sawtooth

float osc_sine(float phase);
float osc_square(float phase);
float osc_noise();
float osc_saw(float phase); // <-- add prototype

#endif