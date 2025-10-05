#pragma once

#include <libremidi/libremidi-c.h>
#include <stddef.h>

struct Synth;

typedef struct {
  int cc;
  const char *param;
  float min, max;
} CCMapping;

extern const CCMapping cc_map[];
extern size_t CC_MAP_SIZE;

typedef struct {
  libremidi_midi_in_handle *in;
  int enabled;
  int last_cc;
  int last_cc_value;
} Midi;

void midi_init(Midi *midi, struct Synth *synth);
void midi_shutdown(Midi *midi);
void midi_map_cc_to_param(struct Synth *synth, int cc, int value);
