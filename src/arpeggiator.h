#pragma once

#define ARP_PATTERN_LEN 16

typedef enum { ARP_UP, ARP_DOWN, ARP_ORDER, ARP_RANDOM } ArpMode;

typedef struct {
    int enabled;
    ArpMode mode;
    int pattern[ARP_PATTERN_LEN];
    int step;
    float tempo;
    float step_phase;
    int held_notes[16];
    int held_count;
    float last_step_time;
} Arpeggiator;

void arpeggiator_init(Arpeggiator *arp);
void arpeggiator_set_param(Arpeggiator *arp, const char *param, float value);
const char *arpeggiator_mode_str(const Arpeggiator *arp);
void arpeggiator_note_on(Arpeggiator *arp, int note);
void arpeggiator_note_off(Arpeggiator *arp, int note);
int arpeggiator_step(Arpeggiator *arp, float samplerate);