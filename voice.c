#include "voice.h"
#include <math.h>

void voice_init(Voice* v) {
    v->note = -1;
    v->phase = 0;
    v->velocity = 0;
    v->on = 0;
    v->waveform = 0;
    v->detune = 0;
    v->osc_phase = 0;
}

void voice_pool_init(Voice* v, int count) {
    for(int i=0;i<count;++i) voice_init(&v[i]);
}

Voice* voice_alloc(Voice* v, int count) {
    for(int i=0;i<count;++i) {
        if(!v[i].on) return &v[i];
    }
    return &v[0]; // Steal the oldest if saturated
}

void voice_note_on(Voice* v, int count, int midi_note, int* osc_wave, float* osc_detune, float* osc_phase) {
    Voice* vv = voice_alloc(v,count);
    vv->note = midi_note;
    vv->phase = 0;
    vv->velocity = 1.0f;
    vv->on = 1;
    vv->waveform = osc_wave ? osc_wave[0] : 0; // Use osc 1 waveform for now
    vv->detune = osc_detune ? osc_detune[0] : 0;
    vv->osc_phase = osc_phase ? osc_phase[0] : 0;
}

void voice_note_off(Voice* v, int count, int midi_note) {
    for(int i=0;i<count;++i) {
        if(v[i].on && v[i].note == midi_note) v[i].on = 0;
    }
}