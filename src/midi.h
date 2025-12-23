#pragma once

#include <stddef.h>

// Forward declarations for libremidi types
typedef struct libremidi_midi_in_handle libremidi_midi_in_handle_t;
typedef struct libremidi_midi_observer_handle libremidi_midi_observer_handle_t;

struct Synth;

typedef struct {
  int cc;
  const char *param;
  float min, max;
} CCMapping;

extern const CCMapping cc_map[];
extern size_t CC_MAP_SIZE;

#define MAX_MIDI_PORTS 64

typedef struct {
  libremidi_midi_in_handle_t *inputs[MAX_MIDI_PORTS];
  libremidi_midi_observer_handle_t *observer;
  int device_count;
  int enabled;
  int last_cc;
  int last_cc_value;
} Midi;

void midi_init(Midi *midi, struct Synth *synth);
void midi_shutdown(Midi *midi);
void midi_map_cc_to_param(struct Synth *synth, int cc, int value);
