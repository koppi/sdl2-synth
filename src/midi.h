#pragma once

struct Synth;

typedef struct {
    void *rtmidi_in;
    int enabled;
    int last_cc;
    int last_cc_value;
} Midi;

void midi_init(Midi *midi, struct Synth *synth);
void midi_shutdown(Midi *midi);
void midi_map_cc_to_param(struct Synth *synth, int cc, int value);