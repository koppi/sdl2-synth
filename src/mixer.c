#include "mixer.h"
#include <string.h>

void mixer_init(Mixer *mixer) {
  for (int i = 0; i < 4; ++i)
    mixer->osc_gain[i] = 0.25f;
  mixer->master = 1.0f;
}

void mixer_apply(Mixer *mixer, float *stereo, int frames) {
  for (int n = 0; n < frames * 2; ++n)
    stereo[n] *= mixer->master;
}

void mixer_set_param(Mixer *mixer, const char *param, float value) {
  if (strncmp(param, "osc", 3) == 0) {
    int i = param[3] - '1';
    if (i >= 0 && i < 4)
      mixer->osc_gain[i] = value;
  } else if (!strcmp(param, "master")) {
    mixer->master = value;
  }
}