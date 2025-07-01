#ifndef VOICE_H
#define VOICE_H

typedef struct Voice {
    int note;
    float phase;
    float velocity;
    int on;
    int waveform;
    float detune;
    float osc_phase;
} Voice;

void voice_init(Voice* v);
void voice_pool_init(Voice* v, int count);
Voice* voice_alloc(Voice* v, int count);
void voice_note_on(Voice* v, int count, int midi_note, int* osc_wave, float* osc_detune, float* osc_phase);
void voice_note_off(Voice* v, int count, int midi_note);

#endif