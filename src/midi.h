#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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

// A runtime (session-only) binding between an incoming MIDI CC and a GUI/synth
// parameter, created via MIDI Learn. `value` points directly at the float or
// int the on-screen knob edits, so applying the mapping is a single write and
// stays in sync with the GUI automatically.
#define MAX_LEARNED_MIDI_MAPPINGS 128

typedef struct {
  int cc;
  int is_int;   // 1 if `value` points to an int, 0 if it points to a float
  void *value;
  float min, max;
  char label[32];
} MidiLearnedMapping;

typedef struct {
  libremidi_midi_in_handle_t *inputs[MAX_MIDI_PORTS];
  libremidi_midi_observer_handle_t *observer;
  int device_count;
  int enabled;
  int last_cc;
  int last_cc_value;

  // MIDI Learn mode: while active, incoming CC messages are captured to bind
  // whichever parameter was last armed (via midi_arm_learn) instead of being
  // applied. While inactive, incoming CC messages apply learned mappings
  // (falling back to the built-in cc_map) as normal.
  int learn_mode;
  void *learn_armed_value;
  int learn_armed_is_int;
  float learn_armed_min, learn_armed_max;
  char learn_armed_label[32];

  MidiLearnedMapping learned[MAX_LEARNED_MIDI_MAPPINGS];
  int learned_count;
} Midi;

// --- Hardware MIDI input (libremidi). Implemented in midi.c, standalone only.
void midi_init(Midi *midi, struct Synth *synth);
void midi_shutdown(Midi *midi);

// --- CC mapping and MIDI Learn. Implemented in midi_map.c (part of the core).
void midi_map_cc_to_param(struct Synth *synth, int cc, int value);

// Arms `value` to be bound to the next incoming MIDI CC (only takes effect
// while midi->learn_mode is set). `value` is a float* if is_int is 0, or an
// int* if is_int is 1.
void midi_arm_learn(Midi *midi, const char *label, void *value, int is_int, float min, float max);
void midi_clear_learned(Midi *midi);
int midi_is_armed(const Midi *midi, const void *value);
const MidiLearnedMapping *midi_find_mapping(const Midi *midi, const void *value);

#ifdef __cplusplus
}
#endif
