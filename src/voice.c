#include "voice.h"
#include <string.h>
#include <math.h>

void voice_init(Voice *v, float samplerate) {
    v->active = 0; v->note = 0; v->velocity = 0; v->env_phase = 0; v->timestamp = 0;
}

void voice_on(Voice *v, float note, float velocity, unsigned long long timestamp) {
    v->active = 1; v->note = note; v->velocity = velocity; v->env_phase = 0; v->timestamp = timestamp;
}

void voice_off(Voice *v) { v->active = 0; }

void voice_render(Voice *v, const Oscillator *osc, float *stereo, int frames) {
    for (int n = 0; n < frames; ++n) {
        float s = 0.0f;
        for (int o = 0; o < 4; ++o)
            s += osc_process((Oscillator*)&osc[o], v->note);
        s *= 0.25f;
        float env = 1.0f - v->env_phase;
        if (env < 0.0f) env = 0.0f;
        s *= env;
        stereo[n*2 + 0] += s;
        stereo[n*2 + 1] += s;
        v->env_phase += 1.0f/48000.0f;
        if (v->env_phase > 1.0f) v->active = 0;
    }
}