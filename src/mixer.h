#pragma once
#include <math.h>

typedef struct {
  // Bus compressor state
  float envelope_L;
  float envelope_R;
  float gain_reduction;
  float attack_coeff;
  float release_coeff;
  int sample_rate;
} BusCompressor;

typedef struct {
  float osc_gain[4];
  float master;
  
  // Bus compressor
  BusCompressor compressor;
  
  // Compressor parameters
  float comp_threshold;    // dB
  float comp_ratio;        // ratio (1:1 to 20:1)
  float comp_attack;       // seconds
  float comp_release;      // seconds
  float comp_makeup_gain;  // dB
  int comp_enabled;
} Mixer;

void mixer_init(Mixer *mixer);
void mixer_apply(Mixer *mixer, float *stereo, int frames);
void mixer_set_param(Mixer *mixer, const char *param, float value);
void mixer_set_sample_rate(Mixer *mixer, int sample_rate);

// Utility functions
float db_to_linear(float db);
float linear_to_db(float linear);