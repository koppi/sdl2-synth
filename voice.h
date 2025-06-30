#ifndef VOICE_H
#define VOICE_H

typedef struct {
    int active;
    int midi;
    float freq;
    float phase;
    float env;
    float env_time;
    int on;
    int osc_mode;
} Voice;

void voice_pool_init(Voice *voices, int count);
void voice_note_on(Voice *voices, int count, int midi_note, int osc_mode);
void voice_note_off(Voice *voices, int count, int midi_note);
void voice_render(Voice *v, float *out, int ns, int osc_mode, int octave, int volume, int samplerate);

#endif