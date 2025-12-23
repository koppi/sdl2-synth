#pragma once
#include <stdint.h>

typedef enum { OSC_SINE, OSC_SAW, OSC_SQUARE, OSC_TRI, OSC_NOISE } OscWaveform;

typedef struct Oscillator {
  OscWaveform waveform;
  float pitch;       // Semitone offset (-24 to +24)
  float phase;       // Phase offset (0 to 1)
  float detune;      // Fine tuning (-1 to +1 semitones)
  float gain;        // Output amplitude (0 to 1)
  float samplerate;  // Audio sample rate
  float phase_acc;   // Internal phase accumulator
  float pan;         // Stereo panning (-1.0 left to +1.0 right, 0.0 center)
  
  // Additional parameters for enhanced control
  float pulse_width; // For square wave pulse width modulation (0.1 to 0.9)
  float unison_detune; // Unison spread amount (0 to 1)
  int unison_voices;   // Number of unison voices (1 to 8)
} Oscillator;

void osc_init(Oscillator *osc, float samplerate);
void osc_set_param(Oscillator *osc, const char *param, float value);
float osc_process(Oscillator *osc, float note, float *phase_acc);