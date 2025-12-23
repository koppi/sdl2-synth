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
  float master_pan;     // Stereo panning for master (-1.0 left to +1.0 right, 0.0 center)
  float master_width;    // Stereo width for master (0.0 mono to 2.0 max)
  
  // Bus compressor
  BusCompressor compressor;
  
  // Compressor parameters
  float comp_threshold;    // dB
  float comp_ratio;        // ratio (1:1 to 20:1)
  float comp_attack;       // seconds
  float comp_release;      // seconds
  float comp_makeup_gain;  // dB
  int comp_enabled;
  
  // Mastering parameters
  int dc_filter_enabled;     // DC offset filter enabled
  float dc_filter_freq;       // DC filter cutoff frequency (Hz)
  int soft_clip_enabled;       // Soft clipping enabled
  float soft_clip_threshold;   // Soft clipping threshold (dB)
  float soft_clip_ratio;        // Soft clipping ratio (knee softness)
  int auto_gain_enabled;        // Auto gain control enabled
  float auto_gain_target;       // Target level for auto gain (dB)
  float peak_level;             // Current peak level (for display)
  float rms_level;              // Current RMS level (for display)
  
  // Auto gain mastering state
  float auto_gain_gain;
  float gain_smoothing_coeff;
  float dc_filter_state_L;
  float dc_filter_state_R;
} Mixer;

void mixer_init(Mixer *mixer);
void mixer_apply(Mixer *mixer, float *stereo, int frames);
void mixer_set_param(Mixer *mixer, const char *param, float value);
void mixer_set_sample_rate(Mixer *mixer, int sample_rate);

// Mastering functions
float dc_filter_process(float input, float *state, float cutoff_freq, float sample_rate);
float soft_clip_process(float input, float threshold_db, float ratio);
void update_level_meters(float *stereo, int frames, float *peak_level, float *rms_level);

// Utility functions
float db_to_linear(float db);
float linear_to_db(float linear);