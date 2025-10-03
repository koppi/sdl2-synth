#pragma once
#include <stdint.h>

typedef enum { OSC_SINE, OSC_SAW, OSC_SQUARE, OSC_TRI } OscWaveform;

typedef struct Oscillator {
  OscWaveform waveform;
  float pitch;
  float phase;
  float detune;
  float gain;
  float samplerate;
  float phase_acc;
} Oscillator;

void osc_init(Oscillator *osc, float samplerate);
void osc_set_param(Oscillator *osc, const char *param, float value);
float osc_process(Oscillator *osc, float note, float *phase_acc);