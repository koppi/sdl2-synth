#include "midi.h"

#include "synth.h"

#include <stdio.h>
#include <string.h>

// MIDI CC -> parameter mapping and MIDI Learn bookkeeping.
//
// This file is part of the synth core and has no dependency on libremidi, so
// it is shared by the standalone app and the VST3 plug-in.

// Standard MIDI CC mapping following MIDI specification
// Uses commonly assigned CC numbers for universal compatibility
const CCMapping cc_map[] = {
    // BANK SELECT & MODULATION (CC 0-31)
    {1, "osc1.pitch", -24.0f, 24.0f},        // CC 1: Modulation Wheel → OSC 1 Pitch Bend
    {2, "osc1.detune", -1.0f, 1.0f},        // CC 2: Breath Controller → OSC 1 Detune
    {7, "osc1.gain", 0.0f, 1.0f},           // CC 7: Volume (Channel Volume) → OSC 1 Gain
    {10, "osc1.waveform", 0.0f, 4.0f},      // CC 10: Pan → OSC 1 Waveform Select
    {11, "osc1.pulse_width", 0.0f, 1.0f},    // CC 11: Expression → OSC 1 Pulse Width
    {12, "osc2.pitch", -24.0f, 24.0f},       // CC 12: Effect Control 1 → OSC 2 Pitch Bend
    {13, "osc2.detune", -1.0f, 1.0f},        // CC 13: Effect Control 2 → OSC 2 Detune
    {14, "osc2.gain", 0.0f, 1.0f},           // CC 14: (Undefined/Unused) → OSC 2 Gain
    {15, "osc3.gain", 0.0f, 1.0f},           // CC 15: (Undefined/Unused) → OSC 3 Gain
    {15, "osc3.waveform", 0.0f, 4.0f},      // CC 15: (Undefined/Unused) → OSC 3 Waveform Select
    {16, "osc4.pitch", -24.0f, 24.0f},       // CC 16: General Purpose Controller 1 → OSC 4 Pitch Bend
    {17, "osc4.detune", -1.0f, 1.0f},        // CC 17: General Purpose Controller 2 → OSC 4 Detune
    
    
    // OSCILLATOR PARAMETER BLOCK (CC 20-31)
    {20, "mixer.master", 0.0f, 2.0f},           // CC 20: (Undefined) → Master Volume
    {21, "fx.filter_cutoff", 20.0f, 20000.0f},   // CC 21: (Undefined) → Filter Cutoff
    {22, "fx.filter_resonance", 0.1f, 10.0f},    // CC 22: (Undefined) → Filter Resonance
    {23, "fx.flanger.rate", 0.1f, 20.0f},       // CC 23: (Undefined) → Flanger Rate
    {24, "fx.delay.time", 0.0f, 1.0f},          // CC 24: (Undefined) → Delay Time
    {25, "fx.reverb.size", 0.0f, 1.0f},          // CC 25: (Undefined) → Reverb Size
    {26, "fx.reverb.mix", 0.0f, 1.0f},            // CC 26: (Undefined) → Reverb Mix
    {27, "fx.flanger.depth", 0.0f, 1.0f},         // CC 27: (Undefined) → Flanger Depth
    {28, "fx.delay.feedback", 0.0f, 1.0f},        // CC 28: (Undefined) → Delay Feedback
    {29, "fx.flanger.feedback", 0.0f, 1.0f},       // CC 29: (Undefined) → Flanger Feedback
    {30, "fx.delay.mix", 0.0f, 1.0f},            // CC 30: (Undefined) → Delay Mix
    
    // STANDARD MIDI CONTROLLERS (CC 32-63)
    {33, "osc4.gain", 0.0f, 1.0f},            // CC 33: (Undefined) → OSC 4 Gain
    {34, "osc4.waveform", 0.0f, 4.0f},        // CC 34: LSB of Bank Select → OSC 4 Waveform
    {35, "osc3.waveform", 0.0f, 4.0f},        // CC 35: LSB of Bank Select → OSC 3 Waveform
    {36, "osc2.waveform", 0.0f, 4.0f},        // CC 36: LSB of Bank Select → OSC 2 Waveform
    {37, "osc2.waveform", 0.0f, 4.0f},        // CC 37: LSB of Bank Select → OSC 2 Waveform
    {38, "osc1.waveform", 0.0f, 4.0f},        // CC 38: LSB of Bank Select → OSC 1 Waveform
    {39, "mixer.comp.threshold", -24.0f, 0.0f},    // CC 39: LSB of Bank Select → Compressor Threshold
    {40, "mixer.comp.ratio", 1.0f, 10.0f},        // CC 40: LSB of Bank Select → Compressor Ratio
    {41, "mixer.comp.attack", 1.0f, 100.0f},        // CC 41: LSB of Bank Select → Compressor Attack
    {42, "mixer.comp.release", 10.0f, 1000.0f},     // CC 42: LSB of Bank Select → Compressor Release
    {43, "mixer.comp.makeup", 0.0f, 12.0f},         // CC 43: LSB of Bank Select → Compressor Makeup
    {44, "fx.filter_drive", 1.0f, 10.0f},          // CC 44: LSB of Bank Select → Filter Drive
    {45, "fx.filter_resonance", 0.1f, 10.0f},        // CC 45: LSB of Bank Select → Filter Resonance
    {46, "fx.filter_mix", 0.0f, 1.0f},             // CC 46: LSB of Bank Select → Filter Mix
    {48, "osc4.unison_voices", 1.0f, 8.0f},        // CC 48: LSB of Bank Select → OSC 4 Unison Voices
    {49, "osc4.unison_voices", 1.0f, 8.0f},        // CC 49: LSB of Bank Select → OSC 4 Unison Voices
    {50, "osc3.unison_voices", 1.0f, 8.0f},        // CC 50: LSB of Bank Select → OSC 3 Unison Voices
    {51, "osc2.unison_voices", 1.0f, 8.0f},        // CC 51: LSB of Bank Select → OSC 2 Unison Voices
    {52, "osc1.unison_voices", 1.0f, 8.0f},        // CC 52: LSB of Bank Select → OSC 1 Unison Voices
    
    // FOOTSWITCH & PEDALS (CC 64-95)
    {64, "sustain_pedal", 0.0f, 1.0f},         // CC 64: Damper Pedal (Sustain) → Sustain On/Off
    {65, "portamento", 0.0f, 1.0f},              // CC 65: Portamento → Portamento On/Off
    {66, "sostenuto", 0.0f, 1.0f},               // CC 66: Sostenuto → Sostenuto On/Off
    {67, "soft_pedal", 0.0f, 1.0f},               // CC 67: Soft Pedal → Soft Pedal On/Off
    {68, "legato_pedal", 0.0f, 1.0f},             // CC 68: Legato Pedal → Legato On/Off
    {69, "hold_pedal", 0.0f, 1.0f},               // CC 69: Hold Pedal → Hold On/Off
    
    // ADDITIONAL EFFECTS & PERFORMANCE CONTROLS (CC 70-95)
    {70, "arp.tempo", 60.0f, 240.0f},             // CC 70: Sound Controller 1 → Arpeggiator Tempo
    {71, "arp.enabled", 0.0f, 1.0f},               // CC 71: Sound Controller 2 → Arpeggiator Enable
    {72, "arp.mode", 0.0f, 17.0f},                 // CC 72: Sound Controller 3 → Arpeggiator Mode (0-17)
    {73, "arp.hold", 0.0f, 1.0f},                  // CC 73: Sound Controller 4 → Arpeggiator Hold
    {74, "arp.octave", 0.0f, 4.0f},                // CC 74: Sound Controller 5 → Arpeggiator Base Octave
    {75, "arp.octaves", 1.0f, 6.0f},                // CC 75: Sound Controller 6 → Arpeggiator Octave Spread
    {76, "fx.delay_feedback", 0.0f, 1.0f},           // CC 76: Sound Controller 7 → Delay Feedback
    {77, "fx.delay.time", 0.0f, 1.0f},              // CC 77: Sound Controller 8 → Delay Time
    {78, "fx.delay.mix", 0.0f, 1.0f},               // CC 78: Sound Controller 9 → Delay Mix
    {79, "fx.flanger.rate", 0.1f, 20.0f},           // CC 79: Sound Controller 10 → Flanger Rate
    {80, "fx.flanger.depth", 0.0f, 1.0f},          // CC 80: Sound Controller 11 → Flanger Depth
    {81, "fx.flanger.feedback", 0.0f, 1.0f},        // CC 81: Sound Controller 12 → Flanger Feedback
    {82, "fx.reverb.size", 0.0f, 1.0f},             // CC 82: Sound Controller 13 → Reverb Size
    {83, "fx.reverb.mix", 0.0f, 1.0f},               // CC 83: Sound Controller 14 → Reverb Mix
    {84, "all_notes_off", 0.0f, 1.0f},              // CC 84: Sound Controller 15 → All Notes Off
    {85, "reset_controllers", 0.0f, 1.0f},           // CC 85: Sound Controller 16 → Reset All Controllers
    
    // CHANNEL MODE & PARAMETER CONTROLS (CC 102-119)
    {102, "arp.polyphonic", 0.0f, 1.0f},           // CC 102: (Undefined) → Arpeggiator Polyphonic Mode
    {103, "arp.tempo", 60.0f, 240.0f},             // CC 103: (Undefined) → Arpeggiator Tempo (Secondary)
    {104, "fx.filter_cutoff", 20.0f, 20000.0f},       // CC 104: (Undefined) → Filter Cutoff (Secondary)
    {105, "fx.filter_resonance", 0.1f, 10.0f},        // CC 105: (Undefined) → Filter Resonance (Secondary)
    {106, "fx.filter_drive", 1.0f, 10.0f},          // CC 106: (Undefined) → Filter Drive (Secondary)
    {107, "fx.filter_mix", 0.0f, 1.0f},               // CC 107: (Undefined) → Filter Mix (Secondary)
    {108, "fx.reverb.size", 0.0f, 1.0f},             // CC 108: (Undefined) → Reverb Size (Secondary)
    {109, "fx.reverb.mix", 0.0f, 1.0f},               // CC 109: (Undefined) → Reverb Mix (Secondary)
    {110, "arp.octave", 0.0f, 4.0f},                // CC 110: (Undefined) → Arpeggiator Base Octave (Secondary)
    {111, "arp.octaves", 1.0f, 6.0f},                // CC 111: (Undefined) → Arpeggiator Octave Spread (Secondary)
    {112, "arp.hold", 0.0f, 1.0f},                  // CC 112: (Undefined) → Arpeggiator Hold (Secondary)
    {113, "arp.enabled", 0.0f, 1.0f},               // CC 113: (Undefined) → Arpeggiator Enable (Secondary)
    {114, "arp.mode", 0.0f, 17.0f},                 // CC 114: (Undefined) → Arpeggiator Mode (Secondary)
    {115, "arp.polyphonic", 0.0f, 1.0f},           // CC 115: (Undefined) → Arpeggiator Polyphonic Mode (Secondary)
    {116, "arp.tempo", 60.0f, 240.0f},             // CC 116: (Undefined) → Arpeggiator Tempo (Secondary)
};
size_t CC_MAP_SIZE = sizeof(cc_map) / sizeof(cc_map[0]);

void midi_arm_learn(Midi *midi, const char *label, void *value, int is_int, float min, float max) {
  midi->learn_armed_value = value;
  midi->learn_armed_is_int = is_int;
  midi->learn_armed_min = min;
  midi->learn_armed_max = max;
  if (label) {
    strncpy(midi->learn_armed_label, label, sizeof(midi->learn_armed_label) - 1);
    midi->learn_armed_label[sizeof(midi->learn_armed_label) - 1] = '\0';
  } else {
    midi->learn_armed_label[0] = '\0';
  }
}

void midi_clear_learned(Midi *midi) {
  midi->learned_count = 0;
  midi->learn_armed_value = NULL;
}

int midi_is_armed(const Midi *midi, const void *value) {
  return midi->learn_armed_value == value && value != NULL;
}

const MidiLearnedMapping *midi_find_mapping(const Midi *midi, const void *value) {
  for (int i = 0; i < midi->learned_count; ++i) {
    if (midi->learned[i].value == value) {
      return &midi->learned[i];
    }
  }
  return NULL;
}

void midi_map_cc_to_param(struct Synth *synth, int cc, int value) {
  Midi *midi = &synth->midi;

  if (midi->learn_mode) {
    // While learning, capture the armed parameter instead of applying it.
    if (midi->learn_armed_value) {
      int idx = -1;
      for (int i = 0; i < midi->learned_count; ++i) {
        if (midi->learned[i].value == midi->learn_armed_value || midi->learned[i].cc == cc) {
          idx = i;
          break;
        }
      }
      if (idx < 0 && midi->learned_count < MAX_LEARNED_MIDI_MAPPINGS) {
        idx = midi->learned_count++;
      }
      if (idx >= 0) {
        midi->learned[idx].cc = cc;
        midi->learned[idx].is_int = midi->learn_armed_is_int;
        midi->learned[idx].value = midi->learn_armed_value;
        midi->learned[idx].min = midi->learn_armed_min;
        midi->learned[idx].max = midi->learn_armed_max;
        strncpy(midi->learned[idx].label, midi->learn_armed_label, sizeof(midi->learned[idx].label) - 1);
        midi->learned[idx].label[sizeof(midi->learned[idx].label) - 1] = '\0';
      }
      midi->learn_armed_value = NULL;
    }
    return;
  }

  // Normal mode: learned mappings (created via MIDI Learn) take priority
  // over the built-in defaults below.
  for (int i = 0; i < midi->learned_count; ++i) {
    if (midi->learned[i].cc == cc) {
      float norm = value / 127.0f;
      float mapped = midi->learned[i].min + (midi->learned[i].max - midi->learned[i].min) * norm;
      if (midi->learned[i].is_int)
        *(int *)midi->learned[i].value = (int)(mapped + (mapped >= 0.0f ? 0.5f : -0.5f));
      else
        *(float *)midi->learned[i].value = mapped;
      return;
    }
  }

  for (size_t i = 0; i < CC_MAP_SIZE; ++i) {
    if (cc_map[i].cc == cc) {
      float norm = value / 127.0f;
      float mapped = cc_map[i].min + (cc_map[i].max - cc_map[i].min) * norm;
      synth_set_param(synth, cc_map[i].param, mapped);
      return;
    }
  }
}
