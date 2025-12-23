#pragma once
#include <stddef.h>

struct Synth; // Forward declaration

typedef enum {
  ARP_OFF,
  ARP_CHORD,
  ARP_UP,
  ARP_DOWN,
  ARP_UP_DOWN,
  ARP_PENDULUM,
  ARP_CONVERGE,
  ARP_DIVERGE,
  ARP_LEAPFROG,
  ARP_THUMB_UP,
  ARP_THUMB_DOWN,
  ARP_PINKY_UP,
  ARP_PINKY_DOWN,
  ARP_REPEAT,
  ARP_RANDOM,
  ARP_RANDOM_WALK,
  ARP_SHUFFLE,
  ARP_ORDER, // Legacy mode for backward compatibility
} ArpMode;

typedef enum {
  RATE_QUARTER = 0,  // 1/4 notes
  RATE_EIGHTH = 1,   // 1/8 notes (default)
  RATE_SIXTEENTH = 2, // 1/16 notes
  RATE_THIRTYSECOND = 3, // 1/32 notes
} ArpRate;

typedef enum {
  CHORD_MAJOR = 0,
  CHORD_MINOR = 1,
  CHORD_SUS = 2,
  CHORD_DIM = 3,
} ChordType;

#define ARP_PATTERN_LEN 16

// Structure to track active arpeggiated notes
typedef struct {
    int note;
    float remaining_duration;
    int active;
} ActiveArpeggiatedNote;

typedef struct {
  int enabled;
  ArpMode mode;
  int pattern[ARP_PATTERN_LEN];
  int step;
  float tempo;
  ArpRate rate;
  float step_phase;
  int held_count;
  int held_notes[16];
  float last_step_time;
  int polyphonic; // New flag for polyphonic arpeggiator
  int hold; // Hold flag to sustain notes after key release
  int octave; // Octave control for keyboard input (0-4 = octaves 1-5)
  int octaves; // Number of octaves to spread chords across (1-6)
  
  // Chord generation parameters
  ChordType chord_type;
  int add_6; // Add 6th extension
  int add_m7; // Add minor 7th extension  
  int add_M7; // Add major 7th extension
  int add_9; // Add 9th extension
  int voicing; // Voicing/inversion level (0-16)
  
  // Gate parameters
  float gate_length; // Gate length (0.0-1.0) - proportion of step time note should sound
  
  ActiveArpeggiatedNote active_arpeggiated_notes[16]; // Max 16 notes in a polyphonic step
} Arpeggiator;

void arpeggiator_init(Arpeggiator *arp);
void arpeggiator_set_param(Arpeggiator *arp, const char *param, float value, struct Synth *synth);
const char *arpeggiator_mode_str(const Arpeggiator *arp);
const char *arpeggiator_rate_str(const Arpeggiator *arp);
const char *arpeggiator_chord_str(const Arpeggiator *arp);
int arpeggiator_generate_chord(const Arpeggiator *arp, int root_note, int *chord_notes, int max_notes);
void arpeggiator_note_on(Arpeggiator *arp, int note);
void arpeggiator_note_off(Arpeggiator *arp, int note);
void arpeggiator_clear_notes(Arpeggiator *arp);
void arpeggiator_clear_notes_with_synth(Arpeggiator *arp, struct Synth *synth);
int arpeggiator_step(Arpeggiator *arp, float samplerate, struct Synth *synth);