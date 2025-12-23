#include "mixer.h"
#include "utils.h"
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
  if (linear <= 0.0f) return -INFINITY;
  return 20.0f * log10f(linear);
}

// DC offset filter to remove DC component
float dc_filter_process(float input, float *state, float cutoff_freq, float sample_rate) {
  // Simple 1-pole high-pass filter for DC removal
  float rc = 1.0f / (2.0f * 3.14159265f * cutoff_freq);
  float alpha = rc / (rc + 1.0f / sample_rate);
  *state += (input - *state) * alpha;
  return *state;
}

// Soft clipping with adjustable knee
float soft_clip_process(float input, float threshold_db, float ratio) {
  float threshold_linear = db_to_linear(threshold_db);
  
  if (fabsf(input) <= threshold_linear) {
    return input; // Below threshold
  }
  
  // Soft knee using power function
  float over_amount = fabsf(input) - threshold_linear;
  float clipped_amount = powf(over_amount / threshold_linear, 1.0f / ratio);
  
  if (input > 0) {
    return threshold_linear + clipped_amount * threshold_linear;
  } else {
    return -(threshold_linear + clipped_amount * threshold_linear);
  }
}

// Update peak and RMS level meters
void update_level_meters(float *stereo, int frames, float *peak_level, float *rms_level) {
  float peak_L = 0.0f, peak_R = 0.0f;
  float sum_L = 0.0f, sum_R = 0.0f;
  
  for (int i = 0; i < frames; i++) {
    float abs_L = fabsf(stereo[i * 2]);
    float abs_R = fabsf(stereo[i * 2 + 1]);
    
    if (abs_L > peak_L) peak_L = abs_L;
    if (abs_R > peak_R) peak_R = abs_R;
    
    sum_L += stereo[i * 2] * stereo[i * 2];
    sum_R += stereo[i * 2 + 1] * stereo[i * 2 + 1];
  }
  
  // RMS calculation
  float rms_L = sqrtf(sum_L / frames);
  float rms_R = sqrtf(sum_R / frames);
  
  // Convert to dB for display
  *peak_level = fmaxf(linear_to_db(peak_L), linear_to_db(peak_R));
  *rms_level = fmaxf(linear_to_db(rms_L), linear_to_db(rms_R));
}

// Calculate envelope detector coefficients
static void calculate_coefficients(BusCompressor *comp, float attack_ms, float release_ms, int sample_rate) {
  comp->attack_coeff = expf(-1.0f / (attack_ms * 0.001f * sample_rate));
  comp->release_coeff = expf(-1.0f / (release_ms * 0.001f * sample_rate));
}

void mixer_init(Mixer *mixer) {
  // Set oscillator gains
  mixer->osc_gain[0] = 1.0f;  // OSC 1 - full volume
  mixer->osc_gain[1] = 0.0f;  // OSC 2 - muted
  mixer->osc_gain[2] = 0.0f;  // OSC 3 - muted
  mixer->osc_gain[3] = 0.0f;  // OSC 4 - muted
  
  mixer->master = 0.25f;  // Reduced from 1.0f to 0.25f
  mixer->master_pan = 0.0f; // Center pan by default
  mixer->master_width = 1.0f; // Full stereo width by default
  
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
  
  // Initialize mastering parameters
  mixer->dc_filter_enabled = 0;        // DC filter disabled by default
  mixer->dc_filter_freq = 20.0f;        // 20Hz DC cutoff
  mixer->soft_clip_enabled = 0;        // Soft clipping disabled by default
  mixer->soft_clip_threshold = -3.0f;   // -3dB soft clipping threshold
  mixer->soft_clip_ratio = 2.0f;          // Soft clipping ratio
  mixer->auto_gain_enabled = 0;         // Auto gain disabled by default
  mixer->auto_gain_target = -6.0f;         // -6dB target level
  mixer->peak_level = 0.0f;
  mixer->rms_level = 0.0f;
  
  // Initialize auto gain state
  mixer->auto_gain_gain = 1.0f;
  mixer->gain_smoothing_coeff = 0.9995f; // More gentle smoothing
  mixer->dc_filter_state_L = 0.0f;
  mixer->dc_filter_state_R = 0.0f;
}

void mixer_set_sample_rate(Mixer *mixer, int sample_rate) {
  mixer->compressor.sample_rate = sample_rate;
  calculate_coefficients(&mixer->compressor, mixer->comp_attack, mixer->comp_release, sample_rate);
  
  // Reset auto gain state when sample rate changes
  mixer->auto_gain_gain = 1.0f;
  mixer->dc_filter_state_L = 0.0f;
  mixer->dc_filter_state_R = 0.0f;
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
  // Calculate levels BEFORE any processing for accurate auto gain measurement
  if (mixer->auto_gain_enabled) {
    update_level_meters(stereo, frames, &mixer->peak_level, &mixer->rms_level);
    
    // Calculate required gain to reach target with safe limits
    float current_level_db = mixer->rms_level;
    float target_level_db = mixer->auto_gain_target;
    float level_diff_db = target_level_db - current_level_db;
    
    // Limit maximum gain change per buffer to prevent clipping
    const float MAX_GAIN_PER_UPDATE = 1.1f; // Maximum +0.83dB per update
    const float MAX_AUTO_GAIN = 4.0f;       // Maximum 12dB total gain
    
    // Calculate safe gain ratio (relative, not absolute)
    float level_ratio = powf(10.0f, level_diff_db / 40.0f); // Half the dB range for safety
    level_ratio = fminf(level_ratio, MAX_GAIN_PER_UPDATE);
    level_ratio = fmaxf(level_ratio, 1.0f / MAX_GAIN_PER_UPDATE); // Also limit reduction
    
    // Smooth gain changes to avoid artifacts
    float smoothing_factor = 1.0f - mixer->gain_smoothing_coeff;
    mixer->auto_gain_gain += (level_ratio - mixer->auto_gain_gain) * smoothing_factor;
    
    // Clamp total gain to prevent clipping
    mixer->auto_gain_gain = fminf(mixer->auto_gain_gain, MAX_AUTO_GAIN);
    mixer->auto_gain_gain = fmaxf(mixer->auto_gain_gain, 0.1f); // Minimum gain
  }
  
  // Apply all processing in single pass for each sample
  for (int n = 0; n < frames * 2; n += 2) { // Process stereo pairs
    // Get current samples
    float left = stereo[n];
    float right = stereo[n + 1];
    
    // Apply master gain first
    left *= mixer->master;
    right *= mixer->master;
    
    // Apply auto gain control (calculated before loop)
    if (mixer->auto_gain_enabled) {
      left *= mixer->auto_gain_gain;
      right *= mixer->auto_gain_gain;
    }
    
    // Apply master panning using constant-power pan law (FIXED: no extra gain)
    float pan = mixer->master_pan;
    float pan_norm = fabsf(pan);
    
    if (pan_norm > 0.0001f) { // Only apply panning if not centered
      // Calculate pan gains using equal-power pan law
      float pan_angle = (pan + 1.0f) * M_PI * 0.25f; // Map -1..1 to 0..PI/2
      float left_gain = fastcos(pan_angle);
      float right_gain = fastsin(pan_angle);
      
      // Apply panning to stereo channels (FIXED: removed sqrtf(2.0f) amplification)
      left *= left_gain;
      right *= right_gain;
    }
    
    // Apply stereo width (mid-side processing) with NO amplification
    float width = mixer->master_width;
    if (width > 0.0001f) { // Only apply width if not mono
      // Calculate mid/side from left/right
      float mid = (left + right) * 0.5f;
      float side = (left - right) * 0.5f;
      
      // Apply width factor (0.0 = mono, 1.0 = normal stereo, 2.0 = wide stereo)
      float width_factor = 1.0f + (width - 1.0f);
      
      // Reconstruct from mid/side (FIXED: removed * 2.0f amplification)
      left = mid - side * width_factor;
      right = mid + side * width_factor;
    }
    
    // Apply bus compressor if enabled
    if (mixer->comp_enabled) {
      float threshold_linear = db_to_linear(mixer->comp_threshold);
      float makeup_linear = db_to_linear(mixer->comp_makeup_gain);
      
      // Process each channel through compressor
      left = process_compressor_channel(&mixer->compressor, left, 
                                                      &mixer->compressor.envelope_L, 
                                                      threshold_linear, mixer->comp_ratio, makeup_linear);
      right = process_compressor_channel(&mixer->compressor, right, 
                                                       &mixer->compressor.envelope_R, 
                                                       threshold_linear, mixer->comp_ratio, makeup_linear);
    }
    
    // Apply DC offset filter
    if (mixer->dc_filter_enabled) {
      left = dc_filter_process(left, &mixer->dc_filter_state_L, mixer->dc_filter_freq, (float)mixer->compressor.sample_rate);
      right = dc_filter_process(right, &mixer->dc_filter_state_R, mixer->dc_filter_freq, (float)mixer->compressor.sample_rate);
    }
    
    // Apply soft clipping
    if (mixer->soft_clip_enabled) {
      left = soft_clip_process(left, mixer->soft_clip_threshold, mixer->soft_clip_ratio);
      right = soft_clip_process(right, mixer->soft_clip_threshold, mixer->soft_clip_ratio);
    }
    
    // FINAL HARD CLIP PROTECTION - prevent exceeding 0dBFS
    left = fmaxf(-1.0f, fminf(1.0f, left));
    right = fmaxf(-1.0f, fminf(1.0f, right));
    
    // Write back to buffer
    stereo[n] = left;
    stereo[n + 1] = right;
  }
}

void mixer_set_param(Mixer *mixer, const char *param, float value) {
  if (strncmp(param, "osc", 3) == 0) {
    int i = param[3] - '1';
    if (i >= 0 && i < 4)
      mixer->osc_gain[i] = value;
  } else if (!strcmp(param, "master")) {
    mixer->master = value;
  } else if (!strcmp(param, "master.pan")) {
    mixer->master_pan = value;
  } else if (!strcmp(param, "master.width")) {
    mixer->master_width = value;
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
  } else if (!strcmp(param, "dc.filter.enabled")) {
    mixer->dc_filter_enabled = (int)value;
  } else if (!strcmp(param, "dc.filter.freq")) {
    mixer->dc_filter_freq = value;
  } else if (!strcmp(param, "soft.clip.enabled")) {
    mixer->soft_clip_enabled = (int)value;
  } else if (!strcmp(param, "soft.clip.threshold")) {
    mixer->soft_clip_threshold = value;
  } else if (!strcmp(param, "soft.clip.ratio")) {
    mixer->soft_clip_ratio = value;
  } else if (!strcmp(param, "auto.gain.enabled")) {
    mixer->auto_gain_enabled = (int)value;
  } else if (!strcmp(param, "auto.gain.target")) {
    mixer->auto_gain_target = value;
  }
}
