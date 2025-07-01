#pragma once

typedef struct {
    float osc_gain[4];
    float master;
} Mixer;

void mixer_init(Mixer *mixer);
void mixer_apply(Mixer *mixer, float *stereo, int frames);
void mixer_set_param(Mixer *mixer, const char *param, float value);