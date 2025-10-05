#pragma once
#include <stddef.h>

struct Synth; // Forward declaration

typedef enum {
  ARP_UP,
  ARP_DOWN,
  ARP_ORDER,
  ARP_RANDOM,
} ArpMode;

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
  float step_phase;
  int held_count;
  int held_notes[16];
  float last_step_time;
  int polyphonic; // New flag for polyphonic arpeggiator
  ActiveArpeggiatedNote active_arpeggiated_notes[16]; // Max 16 notes in a polyphonic step
} Arpeggiator;

void arpeggiator_init(Arpeggiator *arp);
void arpeggiator_set_param(Arpeggiator *arp, const char *param, float value);
const char *arpeggiator_mode_str(const Arpeggiator *arp);
void arpeggiator_note_on(Arpeggiator *arp, int note);
void arpeggiator_note_off(Arpeggiator *arp, int note);
int arpeggiator_step(Arpeggiator *arp, float samplerate, struct Synth *synth);