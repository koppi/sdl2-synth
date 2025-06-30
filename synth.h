#ifndef SYNTH_H
#define SYNTH_H
#include <SDL2/SDL_audio.h>
#include "voice.h"
#include "effects.h"

#define OSC_SINE   1
#define OSC_SQUARE 2

typedef struct {
    SDL_AudioDeviceID audio;
    SDL_AudioSpec spec;
    int osc_mode; // OSC_SINE or OSC_SQUARE
    int octave;
    int volume;
    int demo_playing;
    float flanger_depth, flanger_rate;
    float delay_ms, delay_fb;
    float reverb_mix;
    Voice *voices;
    int voice_count;
    Effects fx; // Embedded effects struct, not pointer
} Synth;

void synth_init(Synth *s);
void synth_close(Synth *s);
void synth_note_on(Synth *s, int midi_note);
void synth_note_off(Synth *s, int midi_note);
void synth_play_demo(Synth *s);
void synth_toggle_osc(Synth *s);
void synth_octave_mod(Synth *s, int delta);
void synth_volume_mod(Synth *s, int delta);
void synth_flanger_mod(Synth *s, int delta);
void synth_delay_mod(Synth *s, int delta);
void synth_state_string(Synth *s, char *buf, int buflen);

#endif