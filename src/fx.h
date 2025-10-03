#pragma once

typedef struct {
  float flanger_depth, flanger_rate, flanger_feedback;
  float delay_time, delay_feedback, delay_mix;
  float reverb_size, reverb_damping, reverb_mix;
  float *delay_buffer;
  int delay_bufsize, delay_pos;
  float *flanger_buffer;
  int flanger_bufsize, flanger_pos;
  int samplerate;
} FX;

void fx_init(FX *fx, int samplerate);
void fx_set_param(FX *fx, const char *param, float value);
void fx_process(FX *fx, float *stereo, int frames);