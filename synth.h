#ifndef SYNTH_H
#define SYNTH_H
#include <SDL2/SDL_audio.h>
#include "voice.h"

#define NUM_OSCS 4
#define NUM_WAVEFORMS 6

enum {
    WAVE_SAW,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_SINE,
    WAVE_PULSE,
    WAVE_NOISE
};

extern const char* waveform_names[NUM_WAVEFORMS];

typedef struct {
    SDL_AudioDeviceID audio;
    SDL_AudioSpec spec;
    int osc_wave[NUM_OSCS];       // 0=Saw, 1=Square, 2=Triangle, 3=Sine, etc.
    float osc_detune[NUM_OSCS];
    float osc_phase[NUM_OSCS];
    int octave;
    int volume;
    int demo_playing;
    float flanger_depth;
    float delay_ms;
    float reverb_mix;
    Voice *voices;
    int voice_count;
    // Demo pattern state
    int demo_step;
    double demo_timer;
    int demo_notes_on[8];
} Synth;

void synth_init(Synth *s);
void synth_close(Synth *s);
void synth_note_on(Synth *s, int midi_note);
void synth_note_off(Synth *s, int midi_note);
void synth_demo_update(Synth *s, double dt);

#endif