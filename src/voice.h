#pragma once
#include "osc.h"
#include "adsr.h"
#include "lfo.h"

typedef struct Voice {
  int active;
  float note;
  float velocity;
  unsigned long long timestamp;
  float phase_acc[6];
  AdsrEnvelope adsr;
} Voice;

void voice_init(Voice *v, float samplerate);
void voice_on(Voice *v, float note, float velocity,
              unsigned long long timestamp);
void voice_off(Voice *v);
void voice_render(Voice *v, const Oscillator *osc, const LFO *lfo, const float *osc_gains, float *stereo, int frames);
int voice_is_active(const Voice *v);