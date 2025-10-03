#include "mixer.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Utility functions
float db_to_linear(float db) {
  return powf(10.0f, db / 20.0f);
}

float linear_to_db(float linear) {
  if (linear <= 0.0f) return -60.0f; // Minimum dB
  return 20.0f * log10f(linear);
}

// Calculate envelope detector coefficients
static void calculate_coefficients(BusCompressor *comp, float attack_ms, float release_ms, int sample_rate) {
  comp->attack_coeff = expf(-1.0f / (attack_ms * 0.001f * sample_rate));
  comp->release_coeff = expf(-1.0f / (release_ms * 0.001f * sample_rate));
}

void mixer_init(Mixer *mixer) {
  for (int i = 0; i < 4; ++i)
    mixer->osc_gain[i] = 0.25f;
  mixer->master = 0.25f;  // Reduced from 1.0f to 0.25f
  
  // Initialize bus compressor
  mixer->compressor.envelope_L = 0.0f;
  mixer->compressor.envelope_R = 0.0f;
  mixer->compressor.gain_reduction = 0.0f;
  mixer->compressor.sample_rate = 48000;
  
  // Default compressor settings (moderate bus compression - DISABLED by default)
  mixer->comp_threshold = -12.0f;    // -12 dB threshold
  mixer->comp_ratio = 3.0f;          // 3:1 ratio
  mixer->comp_attack = 10.0f;        // 10ms attack
  mixer->comp_release = 100.0f;      // 100ms release
  mixer->comp_makeup_gain = 6.0f;    // Increased from 3.0dB to 6.0dB makeup gain
  mixer->comp_enabled = 0;           // Disabled by default (changed from 1 to 0)
  
  calculate_coefficients(&mixer->compressor, mixer->comp_attack, mixer->comp_release, 48000);
}

void mixer_set_sample_rate(Mixer *mixer, int sample_rate) {
  mixer->compressor.sample_rate = sample_rate;
  calculate_coefficients(&mixer->compressor, mixer->comp_attack, mixer->comp_release, sample_rate);
}

static float process_compressor_channel(BusCompressor *comp, float input, float *envelope, 
                                      float threshold_linear, float ratio, float makeup_linear) {
  // Envelope detection (peak detector with attack/release)
  float input_level = fabsf(input);
  
  if (input_level > *envelope) {
    // Attack: faster response to peaks
    *envelope = input_level + ((*envelope - input_level) * comp->attack_coeff);
  } else {
    // Release: slower response to level drops
    *envelope = input_level + ((*envelope - input_level) * comp->release_coeff);
  }
  
  // Compute gain reduction
  float gain_reduction = 1.0f;
  if (*envelope > threshold_linear) {
    // Calculate compression
    float over_threshold = *envelope / threshold_linear;
    float compressed_level = powf(over_threshold, 1.0f / ratio);
    gain_reduction = compressed_level / over_threshold;
  }
  
  // Apply compression and makeup gain
  return input * gain_reduction * makeup_linear;
}

void mixer_apply(Mixer *mixer, float *stereo, int frames) {
  // Apply master volume first
  for (int n = 0; n < frames * 2; ++n)
    stereo[n] *= mixer->master;
  
  // Apply bus compressor if enabled
  if (mixer->comp_enabled) {
    float threshold_linear = db_to_linear(mixer->comp_threshold);
    float makeup_linear = db_to_linear(mixer->comp_makeup_gain);
    
    for (int n = 0; n < frames; ++n) {
      float left = stereo[n * 2 + 0];
      float right = stereo[n * 2 + 1];
      
      // Process each channel through compressor
      stereo[n * 2 + 0] = process_compressor_channel(&mixer->compressor, left, 
                                                     &mixer->compressor.envelope_L, 
                                                     threshold_linear, mixer->comp_ratio, makeup_linear);
      
      stereo[n * 2 + 1] = process_compressor_channel(&mixer->compressor, right, 
                                                     &mixer->compressor.envelope_R, 
                                                     threshold_linear, mixer->comp_ratio, makeup_linear);
    }
    
    // Calculate average gain reduction for metering
    float avg_envelope = 0.5f * (mixer->compressor.envelope_L + mixer->compressor.envelope_R);
    if (avg_envelope > threshold_linear) {
      float over_threshold = avg_envelope / threshold_linear;
      float compressed_level = powf(over_threshold, 1.0f / mixer->comp_ratio);
      mixer->compressor.gain_reduction = linear_to_db(compressed_level / over_threshold);
    } else {
      mixer->compressor.gain_reduction = 0.0f;
    }
  }
}

void mixer_set_param(Mixer *mixer, const char *param, float value) {
  if (strncmp(param, "osc", 3) == 0) {
    int i = param[3] - '1';
    if (i >= 0 && i < 4)
      mixer->osc_gain[i] = value;
  } else if (!strcmp(param, "master")) {
    mixer->master = value;
  } else if (!strcmp(param, "comp.threshold")) {
    mixer->comp_threshold = value;
  } else if (!strcmp(param, "comp.ratio")) {
    mixer->comp_ratio = value;
  } else if (!strcmp(param, "comp.attack")) {
    mixer->comp_attack = value;
    calculate_coefficients(&mixer->compressor, mixer->comp_attack, mixer->comp_release, 
                          mixer->compressor.sample_rate);
  } else if (!strcmp(param, "comp.release")) {
    mixer->comp_release = value;
    calculate_coefficients(&mixer->compressor, mixer->comp_attack, mixer->comp_release, 
                          mixer->compressor.sample_rate);
  } else if (!strcmp(param, "comp.makeup")) {
    mixer->comp_makeup_gain = value;
  } else if (!strcmp(param, "comp.enabled")) {
    mixer->comp_enabled = (int)value;
  }
}