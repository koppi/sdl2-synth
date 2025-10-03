#include "midi.h"
#include "synth.h"
#include <libremidi/libremidi-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example CC mapping table
typedef struct {
  int cc;
  const char *param;
  float min, max;
} CCMapping;

static const CCMapping cc_map[] = {
    {21, "osc1.pitch", -24.0f, 24.0f},
    {22, "osc1.detune", -1.0f, 1.0f},
    {23, "osc1.gain", 0.0f, 1.0f},
    {24, "osc1.phase", 0.0f, 1.0f},
    {25, "osc1.waveform", 0.0f, 3.0f},
    {26, "osc2.pitch", -24.0f, 24.0f},
    {27, "osc2.detune", -1.0f, 1.0f},
    {28, "osc2.gain", 0.0f, 1.0f},
    {29, "osc2.phase", 0.0f, 1.0f},
    {30, "osc2.waveform", 0.0f, 3.0f},
    {40, "fx.delay.mix", 0.0f, 1.0f},
    {41, "fx.delay.feedback", 0.0f, 1.0f},
    {42, "fx.delay.time", 0.0f, 1.0f},
    {43, "fx.flanger.depth", 0.0f, 1.0f},
    {44, "fx.flanger.rate", 0.0f, 5.0f},
    {45, "fx.flanger.feedback", 0.0f, 1.0f},
    {46, "fx.reverb.size", 0.0f, 1.0f},
    {47, "fx.reverb.mix", 0.0f, 1.0f},
    {50, "mixer.osc1", 0.0f, 1.0f},
    {51, "mixer.osc2", 0.0f, 1.0f},
    {52, "mixer.osc3", 0.0f, 1.0f},
    {53, "mixer.osc4", 0.0f, 1.0f},
    {54, "mixer.master", 0.0f, 2.0f},
    {55, "mixer.comp.threshold", -24.0f, 0.0f},
    {56, "mixer.comp.ratio", 1.0f, 10.0f},
    {57, "mixer.comp.attack", 1.0f, 100.0f},
    {58, "mixer.comp.release", 10.0f, 1000.0f},
    {59, "mixer.comp.makeup", 0.0f, 12.0f},
    {60, "arp.tempo", 60.0f, 240.0f},
    {61, "arp.enabled", 0.0f, 1.0f},
    {62, "arp.mode", 0.0f, 3.0f},
};

#define CC_MAP_SIZE (sizeof(cc_map) / sizeof(cc_map[0]))

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