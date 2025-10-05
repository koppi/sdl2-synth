#include "midi.h"
#include "synth.h"
#include <libremidi/libremidi-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example CC mapping table

const CCMapping cc_map[] = {
    {0, "osc1.pitch", -24.0f, 24.0f},
    {1, "osc1.detune", -1.0f, 1.0f},
    {2, "osc1.gain", 0.0f, 1.0f},
    {3, "osc1.phase", 0.0f, 1.0f},
    {4, "osc1.waveform", 0.0f, 4.0f}, // Corrected max to 4.0f
    {5, "osc1.pulse_width", 0.0f, 1.0f},
    {6, "osc1.unison_voices", 1.0f, 8.0f},
    {7, "osc1.unison_detune", 0.0f, 1.0f},

    {8, "osc2.pitch", -24.0f, 24.0f},
    {9, "osc2.detune", -1.0f, 1.0f},
    {10, "osc2.gain", 0.0f, 1.0f},
    {11, "osc2.phase", 0.0f, 1.0f},
    {12, "osc2.waveform", 0.0f, 4.0f}, // Corrected max to 4.0f
    {13, "osc2.pulse_width", 0.0f, 1.0f},
    {14, "osc2.unison_voices", 1.0f, 8.0f},
    {15, "osc2.unison_detune", 0.0f, 1.0f},

    {16, "osc3.pitch", -24.0f, 24.0f},
    {17, "osc3.detune", -1.0f, 1.0f},
    {18, "osc3.gain", 0.0f, 1.0f},
    {19, "osc3.phase", 0.0f, 1.0f},
    {20, "osc3.waveform", 0.0f, 4.0f},
    {21, "osc3.pulse_width", 0.0f, 1.0f},
    {22, "osc3.unison_voices", 1.0f, 8.0f},
    {23, "osc3.unison_detune", 0.0f, 1.0f},

    {24, "osc4.pitch", -24.0f, 24.0f},
    {25, "osc4.detune", -1.0f, 1.0f},
    {26, "osc4.gain", 0.0f, 1.0f},
    {27, "osc4.phase", 0.0f, 1.0f},
    {28, "osc4.waveform", 0.0f, 4.0f},
    {29, "osc4.pulse_width", 0.0f, 1.0f},
    {30, "osc4.unison_voices", 1.0f, 8.0f},
    {31, "osc4.unison_detune", 0.0f, 1.0f},

    // OSC 5 (Bass)
    {53, "osc5.pitch", -24.0f, 24.0f},
    {54, "osc5.detune", -1.0f, 1.0f},
    {55, "osc5.gain", 0.0f, 1.0f},
    {56, "osc5.phase", 0.0f, 1.0f},
    {57, "osc5.waveform", 0.0f, 4.0f},
    {58, "osc5.pulse_width", 0.0f, 1.0f},
    {59, "osc5.unison_voices", 1.0f, 8.0f},
    {60, "osc5.unison_detune", 0.0f, 1.0f},

    // OSC 6 (Percussion)
    {61, "osc6.pitch", -24.0f, 24.0f},
    {62, "osc6.detune", -1.0f, 1.0f},
    {63, "osc6.gain", 0.0f, 1.0f},
    {64, "osc6.phase", 0.0f, 1.0f},
    {65, "osc6.waveform", 0.0f, 4.0f},
    {66, "osc6.pulse_width", 0.0f, 1.0f},
    {67, "osc6.unison_voices", 1.0f, 8.0f},
    {68, "osc6.unison_detune", 0.0f, 1.0f},

    // Existing FX and Mixer mappings (adjust CC numbers if needed to avoid conflicts)
    {69, "fx.delay.mix", 0.0f, 1.0f},
    {70, "fx.delay.feedback", 0.0f, 1.0f},
    {71, "fx.delay.time", 0.0f, 1.0f},
    {72, "fx.flanger.depth", 0.0f, 1.0f},
    {73, "fx.flanger.rate", 0.0f, 5.0f},
    {74, "fx.flanger.feedback", 0.0f, 1.0f},
    {75, "fx.reverb.size", 0.0f, 1.0f},
    {76, "fx.reverb.mix", 0.0f, 1.0f},

    {77, "mixer.osc1", 0.0f, 1.0f},
    {78, "mixer.osc2", 0.0f, 1.0f},
    {79, "mixer.osc3", 0.0f, 1.0f},
    {80, "mixer.osc4", 0.0f, 1.0f},
    {81, "mixer.osc5", 0.0f, 1.0f},
    {82, "mixer.osc6", 0.0f, 1.0f},
    {83, "mixer.master", 0.0f, 2.0f},
    {84, "mixer.comp.threshold", -24.0f, 0.0f},
    {85, "mixer.comp.ratio", 1.0f, 10.0f},
    {86, "mixer.comp.attack", 1.0f, 100.0f},
    {87, "mixer.comp.release", 10.0f, 1000.0f},
    {88, "mixer.comp.makeup", 0.0f, 12.0f},

    {89, "arp.tempo", 60.0f, 240.0f},
    {90, "arp.enabled", 0.0f, 1.0f},
    {91, "arp.mode", 0.0f, 3.0f},
};
size_t CC_MAP_SIZE = sizeof(cc_map) / sizeof(cc_map[0]);



static void midi_callback(void *user, libremidi_timestamp stamp,
                          const unsigned char *msg, size_t len) {
  struct Synth *synth = (struct Synth *)user;
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
    } else if (status == 0x80 || (status == 0x90 && vel == 0)) {
      if (synth->arp.enabled) {
        arpeggiator_note_off(&synth->arp, note);
      } else {
        synth_note_off(synth, note);
      }
    } else if (status == 0xB0) {
      synth_handle_cc(synth, note, vel);
    }
  }
}

struct port_enumeration_data {
  libremidi_midi_in_port* port_to_open;
  int count;
};

// Callback for the enumerator
static void port_enumeration_callback(void* ctx, const libremidi_midi_in_port* port) {
  struct port_enumeration_data* data = (struct port_enumeration_data*)ctx;
  if (data->count == 0) {
    // Clone the first port we see
    libremidi_midi_in_port_clone(port, &data->port_to_open);
  }
  data->count++;

  const char* name_ptr;
  size_t name_len;
  libremidi_midi_in_port_name(port, &name_ptr, &name_len);
  printf("  - Port %d: %.*s\n", data->count - 1, (int)name_len, name_ptr);
}

void midi_init(Midi *midi, struct Synth *synth) {
  // 1. Use the observer API to find ports
  libremidi_observer_configuration obs_config;
  libremidi_midi_observer_configuration_init(&obs_config);
  libremidi_api_configuration api_conf;
  libremidi_midi_api_configuration_init(&api_conf);
  libremidi_midi_observer_handle* observer = NULL;
  libremidi_midi_observer_new(&obs_config, &api_conf, &observer);

  struct port_enumeration_data enum_data = { .port_to_open = NULL, .count = 0 };

  printf("Available MIDI input ports:\n");
  if (observer) {
    libremidi_midi_observer_enumerate_input_ports(observer, &enum_data, port_enumeration_callback);
  }

  if (enum_data.count == 0) {
    printf("  - None\n");
  }

  // 2. Configure the midi_in handle
  libremidi_midi_configuration config;
  libremidi_midi_configuration_init(&config);
  config.on_midi1_message.callback = (void (*)(void*, libremidi_timestamp, const unsigned char*, size_t))midi_callback;
  config.on_midi1_message.context = synth;
  config.version = MIDI1;

  if (enum_data.port_to_open) {
    printf("Opening port 0.\n");
    config.in_port = enum_data.port_to_open;
    config.virtual_port = false;
  } else {
    printf("No MIDI devices found. Creating a virtual port named 'SynthIn'.\n");
    config.port_name = "SynthIn";
    config.virtual_port = true;
  }

  libremidi_midi_in_new(&config, &api_conf, &midi->in);

  // 3. Clean up
  if (enum_data.port_to_open) {
    libremidi_midi_in_port_free(enum_data.port_to_open);
  }
  if (observer) {
    libremidi_midi_observer_free(observer);
  }

  midi->enabled = 1;
  midi->last_cc = -1;
  midi->last_cc_value = 0;
}

void midi_shutdown(Midi *midi) {
  if (midi->in) {
    libremidi_midi_in_free(midi->in);
    midi->in = NULL;
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
