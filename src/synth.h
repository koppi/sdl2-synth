#pragma once
#include <SDL.h>
#include "osc.h"
#include "fx.h"
#include "mixer.h"
#include "voice.h"
#include "arpeggiator.h"
#include "midi.h"

typedef struct Synth {
    Oscillator osc[4];
    Mixer mixer;
    FX fx;
    Voice voices[64];
    int max_voices;
    Arpeggiator arp;
    Midi midi;
    float cpu_usage;
} Synth;

int  synth_init(Synth *synth, int samplerate, int buffer_size, int voices);
void synth_shutdown(Synth *synth);
void synth_audio_callback(void *userdata, Uint8 *stream, int len);
int  synth_active_voices(const Synth *synth);
float synth_cpu_usage(const Synth *synth);
void synth_handle_cc(Synth *synth, int cc, int value);
void synth_set_param(Synth *synth, const char *param, float value);

void synth_note_on(Synth *synth, int note, float velocity);
void synth_note_off(Synth *synth, int note);
