#include "arpeggiator.h"
#include "synth.h" // Include synth.h for synth_note_on/off declarations
#include <stdlib.h>
#include <string.h>

void arpeggiator_init(Arpeggiator *arp) {
  arp->enabled = 0;
  arp->mode = ARP_UP;
  memset(arp->pattern, 0, sizeof(arp->pattern)); // Initialize pattern to all zeros
  arp->step = 0;
  arp->tempo = 120.0f;
  arp->step_phase = 0.0f;
  arp->held_count = 0;
  memset(arp->held_notes, 0, sizeof(arp->held_notes));
  arp->last_step_time = 0.0f;
  arp->polyphonic = 1; // Default to polyphonic
  for (int i = 0; i < 16; ++i) {
      arp->active_arpeggiated_notes[i].active = 0;
  }
}

void arpeggiator_set_param(Arpeggiator *arp, const char *param, float value) {
  if (!strcmp(param, "enabled"))
    arp->enabled = (int)value;
  else if (!strcmp(param, "mode"))
    arp->mode = (ArpMode)((int)value);
  else if (!strcmp(param, "tempo"))
    arp->tempo = value;
  else if (!strcmp(param, "polyphonic"))
    arp->polyphonic = (int)value;
}

const char *arpeggiator_mode_str(const Arpeggiator *arp) {
  switch (arp->mode) {
  case ARP_UP:
    return "UP";
  case ARP_DOWN:
    return "DOWN";
  case ARP_ORDER:
    return "ORDER";
  case ARP_RANDOM:
    return "RANDOM";
  default:
    return "-";
  }
}

void arpeggiator_note_on(Arpeggiator *arp, int note) {
  for (int i = 0; i < arp->held_count; ++i)
    if (arp->held_notes[i] == note)
      return;
  if (arp->held_count < 16)
    arp->held_notes[arp->held_count++] = note;
}

void arpeggiator_note_off(Arpeggiator *arp, int note) {
  for (int i = 0; i < arp->held_count; ++i) {
    if (arp->held_notes[i] == note) {
      for (int j = i; j < arp->held_count - 1; ++j)
        arp->held_notes[j] = arp->held_notes[j + 1];
      arp->held_count--;
      break;
    }
  }
}

static int order_compare(const void *a, const void *b) {
  return (*(int *)a) - (*(int *)b);
}

int arpeggiator_step(Arpeggiator *arp, float samplerate, struct Synth *synth) {
  if (!arp->enabled || arp->held_count == 0)
    return -1;

  float step_time = 60.0f / arp->tempo / 4.0f;
  arp->step_phase += 1.0f / samplerate;

  // Update active arpeggiated notes and turn off expired ones
  for (int i = 0; i < 16; ++i) { // MAX_MELODY_POLYPHONY is 8, but active_arpeggiated_notes is 16
      if (arp->active_arpeggiated_notes[i].active) {
          arp->active_arpeggiated_notes[i].remaining_duration -= 1.0f / samplerate;
          if (arp->active_arpeggiated_notes[i].remaining_duration <= 0) {
              synth_note_off(synth, arp->active_arpeggiated_notes[i].note);
              arp->active_arpeggiated_notes[i].active = 0;
          }
      }
  }

  if (arp->step_phase < step_time)
    return -1;

  arp->step_phase -= step_time;

  // Turn off all notes from the previous step before playing new ones
  for (int i = 0; i < 16; ++i) {
      if (arp->active_arpeggiated_notes[i].active) {
          synth_note_off(synth, arp->active_arpeggiated_notes[i].note);
          arp->active_arpeggiated_notes[i].active = 0;
      }
  }

  int idx = 0;
  int notes[16];
  memcpy(notes, arp->held_notes, sizeof(int) * arp->held_count);

  if (arp->polyphonic) {
      // Play all held notes if polyphonic
      for (int i = 0; i < arp->held_count; ++i) {
          int note_to_play = notes[i];
          // Find a free slot for the new note
          int free_slot = -1;
          for (int j = 0; j < 16; ++j) {
              if (!arp->active_arpeggiated_notes[j].active) {
                  free_slot = j;
                  break;
              }
          }
          if (free_slot != -1) {
              arp->active_arpeggiated_notes[free_slot].note = note_to_play;
              arp->active_arpeggiated_notes[free_slot].remaining_duration = step_time; // Note duration is one step
              arp->active_arpeggiated_notes[free_slot].active = 1;
              synth_note_on(synth, note_to_play, 0.8f);
          }
      }
  } else {
      // Monophonic behavior (existing logic)
      switch (arp->mode) {
      case ARP_UP:
        qsort(notes, arp->held_count, sizeof(int), order_compare);
        idx = arp->step % arp->held_count;
        break;
      case ARP_DOWN:
        qsort(notes, arp->held_count, sizeof(int), order_compare);
        idx = arp->held_count - 1 - (arp->step % arp->held_count);
        break;
      case ARP_ORDER:
        idx = arp->step % arp->held_count;
        break;
      case ARP_RANDOM:
        idx = rand() % arp->held_count;
        break;
      default:
        idx = 0;
      }
      int note_to_play = notes[idx];
      // Find a free slot for the new note
      int free_slot = -1;
      for (int j = 0; j < 16; ++j) {
          if (!arp->active_arpeggiated_notes[j].active) {
              free_slot = j;
              break;
          }
      }
      if (free_slot != -1) {
          arp->active_arpeggiated_notes[free_slot].note = note_to_play;
          arp->active_arpeggiated_notes[free_slot].remaining_duration = step_time; // Note duration is one step
          arp->active_arpeggiated_notes[free_slot].active = 1;
          synth_note_on(synth, note_to_play, 0.8f);
      }
  }

  arp->step++;
  return 0; // Return 0 to indicate success, or the note played if monophonic
}