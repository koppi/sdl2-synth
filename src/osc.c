#include "osc.h"
#include <math.h>
#include <string.h>

void osc_init(Oscillator *osc, float samplerate) {
    osc->waveform = OSC_SAW;
    osc->pitch = 0.0f;
    osc->phase = 0.0f;
    osc->detune = 0.0f;
    osc->gain = 1.0f;
    osc->samplerate = samplerate;
    osc->phase_acc = 0.0f;
}

void osc_set_param(Oscillator *osc, const char *param, float value) {
    if (strcmp(param, "waveform") == 0) osc->waveform = (OscWaveform)((int)value);
    else if (strcmp(param, "pitch") == 0) osc->pitch = value;
    else if (strcmp(param, "phase") == 0) osc->phase = value;
    else if (strcmp(param, "detune") == 0) osc->detune = value;
    else if (strcmp(param, "gain") == 0) osc->gain = value;
}

float osc_process(Oscillator *osc, float note) {
    float freq = 440.0f * powf(2.0f, (note + osc->pitch + osc->detune - 69.0f) / 12.0f);
    osc->phase_acc += freq / osc->samplerate;
    if (osc->phase_acc >= 1.0f) osc->phase_acc -= 1.0f;
    float p = osc->phase_acc + osc->phase;
    if (p >= 1.0f) p -= 1.0f;
    switch (osc->waveform) {
        case OSC_SINE:   return osc->gain * sinf(2.0f * 3.1415926535f * p);
        case OSC_SAW:    return osc->gain * (2.0f * p - 1.0f);
        case OSC_SQUARE: return osc->gain * (p < 0.5f ? 1.0f : -1.0f);
        case OSC_TRI:    return osc->gain * (4.0f * fabsf(p - 0.5f) - 1.0f);
        default: return 0.0f;
    }
}