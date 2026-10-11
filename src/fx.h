#pragma once
#include "analog_filter.h"

#define MAX_DELAY_TAPS 8

typedef struct {
  float flanger_depth, flanger_rate, flanger_feedback;
  float delay_time, delay_feedback, delay_mix;
  float reverb_size, reverb_damping, reverb_mix;
  float *delay_buffer;
  int delay_bufsize, delay_pos;
  float *flanger_buffer;
  int flanger_bufsize, flanger_pos;
  float flanger_phase; // LFO phase accumulator (0..1), persists across blocks
  int samplerate;
  struct ReverbState *reverb; // Per-instance Freeverb-style reverb state
  
  // Multi-tap delay synced to BPM
  float bpm;
  int multitap_enabled;
  float multitap_taps[MAX_DELAY_TAPS];
  float multitap_levels[MAX_DELAY_TAPS];
  int num_taps;
  int multitap_delay_pos;
  
  // Analog filter parameters
  int filter_enabled;
  float filter_cutoff;
  float filter_resonance;
  float filter_drive;
  float filter_mix;
  int filter_oversampling; // Index into {1x, 2x, 4x, 8x}
  AnalogFilter filter;     // Left channel (also holds the shared filter type)
  AnalogFilter filter_r;   // Right channel (identical settings, own state)
} FX;

#ifdef __cplusplus
extern "C" {
#endif

void fx_init(FX *fx, int samplerate);
void fx_set_param(FX *fx, const char *param, float value);
void fx_set_bpm(FX *fx, float bpm);
void fx_process(FX *fx, float *stereo, int frames);
// Clears delay lines, reverb tails and filter state without touching parameters.
void fx_reset(FX *fx);
void fx_cleanup(FX *fx);

#ifdef __cplusplus
}
#endif