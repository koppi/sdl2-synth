#include "midi.h"
#include "synth.h"
#include <rtmidi/rtmidi_c.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Example CC mapping table
typedef struct {
    int cc;
    const char *param;
    float min, max;
} CCMapping;

static const CCMapping cc_map[] = {
    {21, "osc1.pitch",    -24.0f, 24.0f},
    {22, "osc1.detune",   -1.0f, 1.0f},
    {23, "osc1.gain",     0.0f, 1.0f},
    {24, "osc1.phase",    0.0f, 1.0f},
    {25, "osc1.waveform", 0.0f, 3.0f},
    {26, "osc2.pitch",    -24.0f, 24.0f},
    {27, "osc2.detune",   -1.0f, 1.0f},
    {28, "osc2.gain",     0.0f, 1.0f},
    {29, "osc2.phase",    0.0f, 1.0f},
    {30, "osc2.waveform", 0.0f, 3.0f},
    {40, "fx.delay.mix",        0.0f, 1.0f},
    {41, "fx.delay.feedback",   0.0f, 1.0f},
    {42, "fx.delay.time",       0.0f, 1.0f},
    {43, "fx.flanger.depth",    0.0f, 1.0f},
    {44, "fx.flanger.rate",     0.0f, 5.0f},
    {45, "fx.flanger.feedback", 0.0f, 1.0f},
    {46, "fx.reverb.size",      0.0f, 1.0f},
    {47, "fx.reverb.mix",       0.0f, 1.0f},
    {50, "mixer.osc1",   0.0f, 1.0f},
    {51, "mixer.osc2",   0.0f, 1.0f},
    {52, "mixer.osc3",   0.0f, 1.0f},
    {53, "mixer.osc4",   0.0f, 1.0f},
    {54, "mixer.master", 0.0f, 2.0f},
    {60, "arp.tempo",   60.0f, 240.0f},
    {61, "arp.enabled", 0.0f, 1.0f},
    {62, "arp.mode",    0.0f, 3.0f},
};

#define CC_MAP_SIZE (sizeof(cc_map)/sizeof(cc_map[0]))

static void midi_callback(double stamp, const unsigned char *msg, size_t len, void *user) {
    struct Synth *synth = (struct Synth*)user;
    if (len >= 3) {
        unsigned char status = msg[0] & 0xF0;
        unsigned char note = msg[1];
        unsigned char vel = msg[2];
        if (status == 0x90 && vel > 0) {
            if (synth->arp.enabled) {
                arpeggiator_note_on(&synth->arp, note);
            } else {
                synth_note_on(synth, note, vel / 127.0f);
            }
        }
        else if (status == 0x80 || (status == 0x90 && vel == 0)) {
            if (synth->arp.enabled) {
                arpeggiator_note_off(&synth->arp, note);
            } else {
                synth_note_off(synth, note);
            }
        }
        else if (status == 0xB0) {
            synth_handle_cc(synth, note, vel);
        }
    }
}

void midi_init(Midi *midi, struct Synth *synth) {
    midi->rtmidi_in = rtmidi_in_create_default();
    rtmidi_open_port(midi->rtmidi_in, 0, "SynthIn");
    rtmidi_in_set_callback(midi->rtmidi_in, midi_callback, synth);
    midi->enabled = 1;
    midi->last_cc = -1;
    midi->last_cc_value = 0;
}

void midi_shutdown(Midi *midi) {
    if (midi->rtmidi_in) {
        rtmidi_in_free(midi->rtmidi_in);
        midi->rtmidi_in = NULL;
    }
}

void midi_map_cc_to_param(struct Synth *synth, int cc, int value) {
    for (size_t i = 0; i < CC_MAP_SIZE; ++i) {
        if (cc_map[i].cc == cc) {
            float norm = value / 127.0f;
            float mapped = cc_map[i].min + (cc_map[i].max - cc_map[i].min) * norm;
            synth_set_param(synth, cc_map[i].param, mapped);
            return;
        }
    }
    // printf("Unmapped MIDI CC: %d value %d\n", cc, value);
}
