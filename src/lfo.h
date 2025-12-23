#pragma once
#include <stdint.h>

typedef enum { LFO_SINE, LFO_TRIANGLE, LFO_SQUARE, LFO_SAW, LFO_RANDOM } LfoWaveform;

typedef enum { LFO_TARGET_FREQUENCY, LFO_TARGET_FILTER, LFO_TARGET_AMPLITUDE, LFO_TARGET_PAN } LfoTarget;

typedef enum { LFO_SYNC_FREE, LFO_SYNC_RETRIGGER, LFO_SYNC_KEYFOLLOW } LfoSyncMode;

typedef struct LFO {
  LfoWaveform waveform;
  float frequency;    // LFO frequency in Hz (0.1 to 20.0)
  float depth;        // Modulation depth (0.0 to 1.0)
  float phase;        // Phase offset (0 to 1)
  float gain;         // Output amplitude (0 to 1)
  float samplerate;   // Audio sample rate
  float phase_acc;    // Internal phase accumulator
  
  LfoTarget target;   // What parameter to modulate
  LfoSyncMode sync;   // Sync mode (free, retrigger, keyfollow)
  int enabled;        // LFO on/off
  int notes_pressed;  // For keyfollow sync - number of notes currently pressed
} LFO;

void lfo_init(LFO *lfo, float samplerate);
void lfo_set_param(LFO *lfo, const char *param, float value);
float lfo_process(LFO *lfo);
float lfo_get_modulation_value(LFO *lfo);
void lfo_note_on(LFO *lfo);
void lfo_note_off(LFO *lfo);