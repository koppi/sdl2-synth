#include "fx.h"
#include "analog_filter.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Freeverb-style reverb state
#define REVERB_COMBS 4
#define REVERB_ALLPASS 2

typedef struct {
  float *buf;
  int size, pos;
  float feedback;
} Comb;

typedef struct {
  float *buf;
  int size, pos;
  float feedback;
} Allpass;

static void comb_init(Comb *c, int size, float feedback) {
  c->buf = calloc(size, sizeof(float));
  c->size = size;
  c->pos = 0;
  c->feedback = feedback;
}

static void allpass_init(Allpass *a, int size, float feedback) {
  a->buf = calloc(size, sizeof(float));
  a->size = size;
  a->pos = 0;
  a->feedback = feedback;
}

static float comb_process(Comb *c, float inp) {
  float out = c->buf[c->pos];
  c->buf[c->pos] = inp + out * c->feedback;
  c->pos = (c->pos + 1) % c->size;
  return out;
}

static float allpass_process(Allpass *a, float inp) {
  float bufout = a->buf[a->pos];
  float out = -inp + bufout;
  a->buf[a->pos] = inp + bufout * a->feedback;
  a->pos = (a->pos + 1) % a->size;
  return out;
}

struct ReverbState {
  Comb combL[REVERB_COMBS], combR[REVERB_COMBS];
  Allpass allpassL[REVERB_ALLPASS], allpassR[REVERB_ALLPASS];
};

static void reverb_free(struct ReverbState *r) {
  if (!r)
    return;
  for (int i = 0; i < REVERB_COMBS; ++i) {
    free(r->combL[i].buf);
    free(r->combR[i].buf);
  }
  for (int i = 0; i < REVERB_ALLPASS; ++i) {
    free(r->allpassL[i].buf);
    free(r->allpassR[i].buf);
  }
  free(r);
}

static void reverb_clear(struct ReverbState *r) {
  if (!r)
    return;
  for (int i = 0; i < REVERB_COMBS; ++i) {
    memset(r->combL[i].buf, 0, r->combL[i].size * sizeof(float));
    memset(r->combR[i].buf, 0, r->combR[i].size * sizeof(float));
    r->combL[i].pos = r->combR[i].pos = 0;
  }
  for (int i = 0; i < REVERB_ALLPASS; ++i) {
    memset(r->allpassL[i].buf, 0, r->allpassL[i].size * sizeof(float));
    memset(r->allpassR[i].buf, 0, r->allpassR[i].size * sizeof(float));
    r->allpassL[i].pos = r->allpassR[i].pos = 0;
  }
}

void fx_init(FX *fx, int samplerate) {
  fx->samplerate = samplerate;
  fx->flanger_depth = 0.0f;    // Set to 0 (disabled)
  fx->flanger_rate = 0.25f;
  fx->flanger_feedback = 0.5f;
  fx->delay_time = 0.0f;       // Set to 0 (disabled)
  fx->delay_feedback = 0.3f;   // Some feedback for richer delays
  fx->delay_mix = 0.4f;        // Higher mix to hear the effect better
  fx->reverb_size = 0.0f;      // Set to 0.0 (minimal/no reverb)
  fx->reverb_damping = 0.3f;
  fx->reverb_mix = 0.0f;       // Set to 0.0 (100% dry - no reverb in output)
  fx->delay_bufsize = (int)(samplerate * 2.0f);
  fx->delay_buffer = calloc(fx->delay_bufsize * 2, sizeof(float));
  fx->delay_pos = 0;
  fx->flanger_bufsize = (int)(samplerate * 0.05f);
  fx->flanger_buffer = calloc(fx->flanger_bufsize * 2, sizeof(float));
  fx->flanger_pos = 0;
  fx->flanger_phase = 0.0f;
  
  // Initialize multi-tap delay
  fx->bpm = 120.0f;
  fx->multitap_enabled = 0;
  fx->multitap_delay_pos = 0;
  fx->num_taps = 4;
  // Default tap delays in beats: quarter, dotted eighth, eighth, triplet
  float tap_beats[] = {1.0f, 0.75f, 0.5f, 0.333f};
  float tap_levels[] = {0.7f, 0.5f, 0.4f, 0.3f};
  for (int i = 0; i < MAX_DELAY_TAPS; i++) {
    if (i < 4) {
      fx->multitap_taps[i] = tap_beats[i];
      fx->multitap_levels[i] = tap_levels[i];
    } else {
      fx->multitap_taps[i] = 0.0f;
      fx->multitap_levels[i] = 0.0f;
    }
  }
  
  // Initialize analog filter
  fx->filter_enabled = 0;       // Disabled by default
  fx->filter_cutoff = 1000.0f;  // 1kHz default
  fx->filter_resonance = 1.0f; // Q = 1.0 default
  fx->filter_drive = 1.0f;     // No drive default
  fx->filter_mix = 1.0f;        // 100% wet default
  fx->filter_oversampling = 1; // Index 1 = 2x oversampling default
  analog_filter_init(&fx->filter, samplerate);
  analog_filter_init(&fx->filter_r, samplerate);

  fx->reverb = calloc(1, sizeof(struct ReverbState));
  if (fx->reverb) {
    struct ReverbState *r = fx->reverb;
    int comb_delays[REVERB_COMBS] = {
        (int)(0.0297f * samplerate), (int)(0.0371f * samplerate),
        (int)(0.0411f * samplerate), (int)(0.0437f * samplerate)};
    int allpass_delays[REVERB_ALLPASS] = {(int)(0.005f * samplerate),
                                          (int)(0.0017f * samplerate)};
    float comb_feedback = 0.805f;
    float allpass_feedback = 0.7f;
    for (int i = 0; i < REVERB_COMBS; ++i) {
      comb_init(&r->combL[i], comb_delays[i], comb_feedback);
      comb_init(&r->combR[i], comb_delays[i] + 23, comb_feedback);
    }
    for (int i = 0; i < REVERB_ALLPASS; ++i) {
      allpass_init(&r->allpassL[i], allpass_delays[i], allpass_feedback);
      allpass_init(&r->allpassR[i], allpass_delays[i] + 13, allpass_feedback);
    }
  }
}

void fx_set_param(FX *fx, const char *param, float value) {
  if (!strcmp(param, "flanger.depth"))
    fx->flanger_depth = value;
  else if (!strcmp(param, "flanger.rate"))
    fx->flanger_rate = value;
  else if (!strcmp(param, "flanger.feedback"))
    fx->flanger_feedback = value;
  else if (!strcmp(param, "delay.time"))
    fx->delay_time = value;
  else if (!strcmp(param, "delay.feedback"))
    fx->delay_feedback = value;
  else if (!strcmp(param, "delay.mix"))
    fx->delay_mix = value;
  else if (!strcmp(param, "reverb.size"))
    fx->reverb_size = value;
  else if (!strcmp(param, "reverb.damping"))
    fx->reverb_damping = value;
  else if (!strcmp(param, "reverb.mix"))
    fx->reverb_mix = value;
  // Multi-tap delay parameters
  else if (!strcmp(param, "multitap.enabled"))
    fx->multitap_enabled = (int)value;
  else if (!strcmp(param, "multitap.bpm"))
    fx->bpm = value;
  else if (!strncmp(param, "multitap.tap", 12)) {
    // Parse tap index: multitap.tap0, multitap.tap1, etc.
    int tap_idx = atoi(param + 12);
    if (tap_idx >= 0 && tap_idx < MAX_DELAY_TAPS) {
      if (!strncmp(param + 13, "_level", 6)) {
        fx->multitap_levels[tap_idx] = value;
      } else {
        fx->multitap_taps[tap_idx] = value;
      }
    }
  }
  // Analog filter parameters
  else if (!strcmp(param, "filter.enabled"))
    fx->filter_enabled = (int)value;
  else if (!strcmp(param, "filter.cutoff"))
    fx->filter_cutoff = value;
  else if (!strcmp(param, "filter.resonance"))
    fx->filter_resonance = value;
  else if (!strcmp(param, "filter.drive"))
    fx->filter_drive = value;
  else if (!strcmp(param, "filter.mix"))
    fx->filter_mix = value;
  else if (!strcmp(param, "filter.oversampling"))
    fx->filter_oversampling = (int)value;
  // Pass through to analog filter
  else if (!strncmp(param, "filter.", 7)) {
    analog_filter_set_param(&fx->filter, param + 7, value);
  }
}

// Upper bound on frames handled in one pass so the filter's scratch buffers can
// live on the stack (fx_process never allocates).
#define FX_BLOCK_FRAMES 256

static void fx_process_block(FX *fx, float *stereo, int frames);

void fx_process(FX *fx, float *stereo, int frames) {
  while (frames > 0) {
    int n = frames > FX_BLOCK_FRAMES ? FX_BLOCK_FRAMES : frames;
    fx_process_block(fx, stereo, n);
    stereo += n * 2;
    frames -= n;
  }
}

static void fx_process_block(FX *fx, float *stereo, int frames) {
  // Apply analog filter first if enabled
  if (fx->filter_enabled) {
    // Both channels share the user-facing settings but keep separate state.
    fx->filter_r.type = fx->filter.type;
    AnalogFilter *filters[2] = {&fx->filter, &fx->filter_r};
    // The oversampling parameter is an index into {1x, 2x, 4x, 8x}.
    int oversampling = 1 << (fx->filter_oversampling < 0 ? 0 : (fx->filter_oversampling > 3 ? 3 : fx->filter_oversampling));

    for (int ch = 0; ch < 2; ++ch) {
      AnalogFilter *f = filters[ch];
      analog_filter_set_param(f, "cutoff", fx->filter_cutoff);
      analog_filter_set_param(f, "resonance", fx->filter_resonance);
      analog_filter_set_param(f, "drive", fx->filter_drive);
      analog_filter_set_param(f, "mix", fx->filter_mix);
      analog_filter_set_param(f, "oversampling", (float)oversampling);

      float channel[FX_BLOCK_FRAMES];
      float filtered[FX_BLOCK_FRAMES];
      for (int i = 0; i < frames; i++) {
        channel[i] = stereo[i * 2 + ch];
      }
      analog_filter_process(f, channel, filtered, frames);
      for (int i = 0; i < frames; i++) {
        stereo[i * 2 + ch] = filtered[i];
      }
    }
  }
  
  for (int n = 0; n < frames; ++n) {
    fx->flanger_phase += fx->flanger_rate / fx->samplerate;
    if (fx->flanger_phase >= 1.0f)
      fx->flanger_phase -= 1.0f;
    float phase = fastsin(2.0f * 3.14159265f * fx->flanger_phase);
    int delay_samp = (int)(fx->flanger_depth * (fx->flanger_bufsize - 1) *
                           (0.5f + 0.5f * phase));
    int pos = (fx->flanger_pos + fx->flanger_bufsize - delay_samp) %
              fx->flanger_bufsize;
    for (int ch = 0; ch < 2; ++ch) {
      float in = stereo[n * 2 + ch];
      float fb = fx->flanger_buffer[pos * 2 + ch];
      stereo[n * 2 + ch] = in + fb * fx->flanger_feedback;
      fx->flanger_buffer[fx->flanger_pos * 2 + ch] = in;
    }
    fx->flanger_pos = (fx->flanger_pos + 1) % fx->flanger_bufsize;
  }
  // Multi-tap delay synced to BPM (replaces standard delay when enabled)
  if (fx->multitap_enabled && fx->delay_mix > 0.0f) {
    float seconds_per_beat = 60.0f / fx->bpm;
    
    for (int n = 0; n < frames; ++n) {
      float dryL = stereo[n * 2 + 0];
      float dryR = stereo[n * 2 + 1];
      float wetL = 0.0f, wetR = 0.0f;
      
      // Sum all taps
      for (int tap = 0; tap < fx->num_taps; ++tap) {
        if (fx->multitap_levels[tap] > 0.0f && fx->multitap_taps[tap] > 0.0f) {
          float delay_seconds = fx->multitap_taps[tap] * seconds_per_beat;
          int delay_samp = (int)(delay_seconds * fx->samplerate);
          
          // Ensure delay is within buffer bounds
          if (delay_samp > 0 && delay_samp < fx->delay_bufsize) {
            int pos = (fx->delay_pos + fx->delay_bufsize - delay_samp) % fx->delay_bufsize;
            
            // Read delayed signal
            float tapL = fx->delay_buffer[pos * 2 + 0];
            float tapR = fx->delay_buffer[pos * 2 + 1];
            
            // Add to wet mix with tap level
            wetL += tapL * fx->multitap_levels[tap];
            wetR += tapR * fx->multitap_levels[tap];
          }
        }
      }
      
      // Mix wet and dry
      stereo[n * 2 + 0] = dryL * (1.0f - fx->delay_mix) + wetL * fx->delay_mix;
      stereo[n * 2 + 1] = dryR * (1.0f - fx->delay_mix) + wetR * fx->delay_mix;
      
      // Write input to buffer (ALWAYS write dry signal to keep buffer filled)
      fx->delay_buffer[fx->delay_pos * 2 + 0] = dryL;
      fx->delay_buffer[fx->delay_pos * 2 + 1] = dryR;
      fx->delay_pos = (fx->delay_pos + 1) % fx->delay_bufsize;
    }
  } else {
    // Standard delay (original behavior)
    for (int n = 0; n < frames; ++n) {
      int delay_samp = (int)(fx->delay_time * fx->samplerate);
      int pos =
          (fx->delay_pos + fx->delay_bufsize - delay_samp) % fx->delay_bufsize;
      for (int ch = 0; ch < 2; ++ch) {
        float dry = stereo[n * 2 + ch];
        float wet = fx->delay_buffer[pos * 2 + ch];
        stereo[n * 2 + ch] = dry * (1.0f - fx->delay_mix) + wet * fx->delay_mix;
        fx->delay_buffer[fx->delay_pos * 2 + ch] = dry + wet * fx->delay_feedback;
      }
      fx->delay_pos = (fx->delay_pos + 1) % fx->delay_bufsize;
    }
  }
  for (int n = 0; n < frames; ++n) {
    float inL = stereo[n * 2 + 0];
    float inR = stereo[n * 2 + 1];
    float reverb_in = (inL + inR) * 0.5f * fx->reverb_size;

    float outL = 0, outR = 0;
    struct ReverbState *r = fx->reverb;
    if (r) {
      for (int i = 0; i < REVERB_COMBS; ++i) {
        outL += comb_process(&r->combL[i], reverb_in);
        outR += comb_process(&r->combR[i], reverb_in);
      }
      for (int i = 0; i < REVERB_ALLPASS; ++i) {
        outL = allpass_process(&r->allpassL[i], outL);
        outR = allpass_process(&r->allpassR[i], outR);
      }
    }
    stereo[n * 2 + 0] = inL * (1.0f - fx->reverb_mix) + outL * fx->reverb_mix;
    stereo[n * 2 + 1] = inR * (1.0f - fx->reverb_mix) + outR * fx->reverb_mix;
  }
}

void fx_set_bpm(FX *fx, float bpm) {
  fx->bpm = bpm;
}

void fx_reset(FX *fx) {
  if (fx->delay_buffer)
    memset(fx->delay_buffer, 0, (size_t)fx->delay_bufsize * 2 * sizeof(float));
  if (fx->flanger_buffer)
    memset(fx->flanger_buffer, 0, (size_t)fx->flanger_bufsize * 2 * sizeof(float));
  fx->delay_pos = 0;
  fx->flanger_pos = 0;
  fx->flanger_phase = 0.0f;
  fx->multitap_delay_pos = 0;
  reverb_clear(fx->reverb);

  AnalogFilter *filters[2] = {&fx->filter, &fx->filter_r};
  for (int i = 0; i < 2; ++i) {
    filters[i]->x1 = filters[i]->x2 = filters[i]->y1 = filters[i]->y2 = 0.0f;
    filters[i]->sat_state = 0.0f;
  }
}

void fx_cleanup(FX *fx) {
  if (fx->delay_buffer) {
    free(fx->delay_buffer);
    fx->delay_buffer = NULL;
  }
  if (fx->flanger_buffer) {
    free(fx->flanger_buffer);
    fx->flanger_buffer = NULL;
  }
  reverb_free(fx->reverb);
  fx->reverb = NULL;
  // Clean up analog filters
  analog_filter_cleanup(&fx->filter);
  analog_filter_cleanup(&fx->filter_r);
}
