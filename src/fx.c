#include "fx.h"
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

typedef struct {
  Comb combL[REVERB_COMBS], combR[REVERB_COMBS];
  Allpass allpassL[REVERB_ALLPASS], allpassR[REVERB_ALLPASS];
  int initialized;
} ReverbState;

static ReverbState reverb_state = {0};

void fx_init(FX *fx, int samplerate) {
  fx->samplerate = samplerate;
  fx->flanger_depth = 0.0f;    // Set to 0 (disabled)
  fx->flanger_rate = 0.25f;
  fx->flanger_feedback = 0.5f;
  fx->delay_time = 0.0f;       // Set to 0 (disabled)
  fx->delay_feedback = 0.0f;   // Set to 0 (disabled)
  fx->delay_mix = 0.25f;
  fx->reverb_size = 0.0f;      // Set to 0.0 (minimal/no reverb)
  fx->reverb_damping = 0.3f;
  fx->reverb_mix = 0.0f;       // Set to 0.0 (100% dry - no reverb in output)
  fx->delay_bufsize = (int)(samplerate * 2.0f);
  fx->delay_buffer = calloc(fx->delay_bufsize * 2, sizeof(float));
  fx->delay_pos = 0;
  fx->flanger_bufsize = (int)(samplerate * 0.05f);
  fx->flanger_buffer = calloc(fx->flanger_bufsize * 2, sizeof(float));
  fx->flanger_pos = 0;

  if (!reverb_state.initialized) {
    int comb_delays[REVERB_COMBS] = {
        (int)(0.0297f * samplerate), (int)(0.0371f * samplerate),
        (int)(0.0411f * samplerate), (int)(0.0437f * samplerate)};
    int allpass_delays[REVERB_ALLPASS] = {(int)(0.005f * samplerate),
                                          (int)(0.0017f * samplerate)};
    float comb_feedback = 0.805f;
    float allpass_feedback = 0.7f;
    for (int i = 0; i < REVERB_COMBS; ++i) {
      comb_init(&reverb_state.combL[i], comb_delays[i], comb_feedback);
      comb_init(&reverb_state.combR[i], comb_delays[i] + 23, comb_feedback);
    }
    for (int i = 0; i < REVERB_ALLPASS; ++i) {
      allpass_init(&reverb_state.allpassL[i], allpass_delays[i],
                   allpass_feedback);
      allpass_init(&reverb_state.allpassR[i], allpass_delays[i] + 13,
                   allpass_feedback);
    }
    reverb_state.initialized = 1;
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
}

void fx_process(FX *fx, float *stereo, int frames) {
  for (int n = 0; n < frames; ++n) {
    float phase =
        sinf(2.0f * 3.14159265f * fx->flanger_rate * n / fx->samplerate);
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
  for (int n = 0; n < frames; ++n) {
    float inL = stereo[n * 2 + 0];
    float inR = stereo[n * 2 + 1];
    float reverb_in = (inL + inR) * 0.5f * fx->reverb_size;

    float outL = 0, outR = 0;
    for (int i = 0; i < REVERB_COMBS; ++i) {
      outL += comb_process(&reverb_state.combL[i], reverb_in);
      outR += comb_process(&reverb_state.combR[i], reverb_in);
    }
    for (int i = 0; i < REVERB_ALLPASS; ++i) {
      outL = allpass_process(&reverb_state.allpassL[i], outL);
      outR = allpass_process(&reverb_state.allpassR[i], outR);
    }
    stereo[n * 2 + 0] = inL * (1.0f - fx->reverb_mix) + outL * fx->reverb_mix;
    stereo[n * 2 + 1] = inR * (1.0f - fx->reverb_mix) + outR * fx->reverb_mix;
  }
}