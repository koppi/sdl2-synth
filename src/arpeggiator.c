#include "arpeggiator.h"
#include <stdlib.h>
#include <string.h>

void arpeggiator_init(Arpeggiator *arp) {
  arp->enabled = 0;
  arp->mode = ARP_UP;
  for (int i = 0; i < ARP_PATTERN_LEN; ++i)
    arp->pattern[i] = 1;
  arp->step = 0;
  arp->tempo = 120.0f;
  arp->step_phase = 0.0f;
  arp->held_count = 0;
  memset(arp->held_notes, 0, sizeof(arp->held_notes));
  arp->last_step_time = 0.0f;
}

void arpeggiator_set_param(Arpeggiator *arp, const char *param, float value) {
  if (!strcmp(param, "enabled"))
    arp->enabled = (int)value;
  else if (!strcmp(param, "mode"))
    arp->mode = (ArpMode)((int)value);
  else if (!strcmp(param, "tempo"))
    arp->tempo = value;
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

int arpeggiator_step(Arpeggiator *arp, float samplerate) {
  if (!arp->enabled || arp->held_count == 0)
    return -1;
  float step_time = 60.0f / arp->tempo / 4.0f;
  arp->step_phase += 1.0f / samplerate;
  if (arp->step_phase < step_time)
    return -1;

  arp->step_phase -= step_time;
  int idx = 0;
  int notes[16];
  memcpy(notes, arp->held_notes, sizeof(int) * arp->held_count);
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
  arp->step++;
  return notes[idx];
}