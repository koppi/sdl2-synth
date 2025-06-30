#include "voice.h"
#include "osc.h"
#include <math.h>
#include <string.h>

#define ENV_ATTACK 0.01f
#define ENV_RELEASE 0.18f

static float midi2freq(int midi) {
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

void voice_pool_init(Voice *voices, int count) {
    memset(voices, 0, sizeof(Voice)*count);
}

void voice_note_on(Voice *voices, int count, int midi_note, int osc_mode) {
    for (int i=0; i<count; ++i) {
        if (!voices[i].active) {
            voices[i].active = 1;
            voices[i].midi = midi_note;
            voices[i].freq = midi2freq(midi_note);
            voices[i].phase = 0.0f;
            voices[i].env = 0.0f;
            voices[i].on = 1;
            voices[i].osc_mode = osc_mode;
            voices[i].env_time = 0.0f;
            return;
        }
    }
    // Steal oldest
    voices[0].active = 1;
    voices[0].midi = midi_note;
    voices[0].freq = midi2freq(midi_note);
    voices[0].phase = 0.0f;
    voices[0].env = 0.0f;
    voices[0].on = 1;
    voices[0].osc_mode = osc_mode;
    voices[0].env_time = 0.0f;
}

void voice_note_off(Voice *voices, int count, int midi_note) {
    for (int i=0; i<count; ++i) {
        if (voices[i].active && voices[i].midi == midi_note) {
            voices[i].on = 0;
            voices[i].env_time = 0.0f;
        }
    }
}

void voice_render(Voice *v, float *out, int ns, int osc_mode, int octave, int volume, int samplerate) {
    if (!v->active) return;
    float freq = v->freq * powf(2.0f, octave-4);
    float phase = v->phase;
    float env = v->env;
    float env_time = v->env_time;
    int on = v->on;
    float gain = (float)volume/127.0f * 0.15f;
    for (int i=0; i<ns; ++i) {
        // Envelope
        if (on && env < 1.0f) {
            env += 1.0f/(ENV_ATTACK*samplerate);
            if (env > 1.0f) env = 1.0f;
        }
        if (!on && env > 0.0f) {
            env -= 1.0f/(ENV_RELEASE*samplerate);
            if (env < 0.0f) env = 0.0f;
        }
        float s = 0.0f;
        if (osc_mode == OSC_SINE) s = osc_sine(phase);
        else if (osc_mode == OSC_SQUARE) s = osc_square(phase);
        else s = (osc_sine(phase)+osc_square(phase))*0.5f;
        s *= gain * env;

        out[i*2+0] += s;
        out[i*2+1] += s;

        // Advance phase
        phase += freq/(float)samplerate;
        if (phase >= 1.0f) phase -= 1.0f;
        env_time += 1.0f/(float)samplerate;
    }
    v->phase = phase;
    v->env = env;
    v->env_time = env_time;
    if (env <= 0.0f) v->active = 0;
}