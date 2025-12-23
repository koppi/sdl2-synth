#include "midi.h"

#include "synth.h"
#include "arpeggiator.h"

// Include libremidi headers - they appear to be found by the build system
#include <libremidi/libremidi-c.h>
#include <libremidi/api-c.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    {14, "osc3.gain", 0.0f, 1.0f},           // CC 14: (Undefined/Unused) → OSC 3 Gain
    {15, "osc3.waveform", 0.0f, 4.0f},      // CC 15: (Undefined/Unused) → OSC 3 Waveform Select
    {16, "osc4.pitch", -24.0f, 24.0f},       // CC 16: General Purpose Controller 1 → OSC 4 Pitch Bend
    {17, "osc4.detune", -1.0f, 1.0f},        // CC 17: General Purpose Controller 2 → OSC 4 Detune
    {18, "osc5.gain", 0.0f, 1.0f},           // CC 18: General Purpose Controller 3 → OSC 5 Gain
    {19, "osc6.gain", 0.0f, 1.0f},           // CC 19: General Purpose Controller 4 → OSC 6 Gain
    
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
    {33, "osc6.waveform", 0.0f, 4.0f},        // CC 33: LSB of Bank Select → OSC 6 Waveform
    {34, "osc5.waveform", 0.0f, 4.0f},        // CC 34: LSB of Bank Select → OSC 5 Waveform
    {35, "osc4.waveform", 0.0f, 4.0f},        // CC 35: LSB of Bank Select → OSC 4 Waveform
    {36, "osc3.waveform", 0.0f, 4.0f},        // CC 36: LSB of Bank Select → OSC 3 Waveform
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
    {47, "osc6.unison_voices", 1.0f, 8.0f},        // CC 47: LSB of Bank Select → OSC 6 Unison Voices
    {48, "osc5.unison_voices", 1.0f, 8.0f},        // CC 48: LSB of Bank Select → OSC 5 Unison Voices
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

// Enhanced MIDI callback with comprehensive logging
void on_midi1_message(void* ctx, libremidi_timestamp ts, const libremidi_midi1_symbol* msg, size_t len) {
  struct Synth *synth = (struct Synth *)ctx;
  
   // Defensive check to prevent processing invalid messages
  if (!msg || !synth || len < 3) {
    SDL_Log("ERROR: Invalid MIDI message: msg=%p, len=%zu, synth=%p\n", (void*)msg, (size_t)len, (void*)synth);
    return;
  }
   
  // Additional safety check: verify message pointer validity before accessing
  if (len < 3 || len > 256) {
    SDL_Log("WARNING: Invalid MIDI message - len=%zu, msg=%p\n", (size_t)len, (void*)msg);
    return;
  }
  
  if (len >= 3) {
    unsigned char status = msg[0] & 0xF0;
    unsigned char note = msg[1];
    unsigned char vel = msg[2];
    
    switch (status) {
      case 0x80: // Note Off
        if (synth->arp.enabled) {
          arpeggiator_note_off(&synth->arp, note);
        } else {
          synth_note_off(synth, note);
        }
        break;
        
      case 0x90: // Note On (with velocity)
        if (synth->arp.enabled) {
          arpeggiator_note_on(&synth->arp, note);
        } else {
          synth_note_on(synth, note, vel / 127.0f);
        }
        break;
        
      case 0xB0: // Control Change
        // Store last CC info for GUI display
        synth->midi.last_cc = note;
        synth->midi.last_cc_value = vel;
        
        // Map CC to synth parameters
        midi_map_cc_to_param(synth, note, vel);
        break;
        
      default:
        SDL_Log("Unknown Status: 0x%02X\n", status);
        break;
    }
  }
}

// Structure to store enumerated ports
typedef struct {
  libremidi_midi_in_port* ports[MAX_MIDI_PORTS];
  int count;
} midi_enumerated_ports;

// Callback for when input ports are found
void on_input_port_found(void* ctx, const libremidi_midi_in_port* port) {
  midi_enumerated_ports* e = (midi_enumerated_ports*)ctx;
  if (e->count < MAX_MIDI_PORTS) {
    libremidi_midi_in_port_clone(port, &e->ports[e->count]);
    e->count++;
  }

  const char* name = NULL;
  size_t len = 0;
  if (libremidi_midi_in_port_name(port, &name, &len) == 0) {
    SDL_Log("  - Found MIDI input: %.*s\n", (int)len, name);
  }
}

// WebMIDI callback for when devices are added dynamically
void webmidi_input_added(void* ctx, const libremidi_midi_in_port* port) {
  if (!ctx) {
    SDL_Log("ERROR: webmidi_input_added called with null context\n");
    return;
  }
  
  struct Synth *synth = (struct Synth *)ctx;
  if (!synth) {
    SDL_Log("ERROR: webmidi_input_added called with null synth\n");
    return;
  }
  
  Midi *midi = &synth->midi;
  if (!midi) {
    SDL_Log("ERROR: webmidi_input_added called with null midi\n");
    return;
  }
  
  if (midi->device_count >= MAX_MIDI_PORTS) {
    SDL_Log("WARNING: Maximum MIDI devices reached, ignoring new device\n");
    return;
  }
  
  const char* name = NULL;
  size_t len = 0;
  if (libremidi_midi_in_port_name(port, &name, &len) != 0) {
    SDL_Log("[WebMIDI] Input device connected (unknown name)\n");
    return;
  }
  
  SDL_Log("[WebMIDI] Input device connected: %.*s\n", (int)len, name);
  
   // Clone the port
  libremidi_midi_in_port* port_clone;
  if (libremidi_midi_in_port_clone(port, &port_clone) != 0) {
    SDL_Log("ERROR: Failed to clone MIDI port\n");
    return;
  }
  
  // Validate port clone before use
  if (!port_clone) {
    SDL_Log("ERROR: Port clone resulted in null pointer\n");
    return;
  }
  
  // Validate port clone before use
  if (!port_clone) {
    SDL_Log("ERROR: Port clone resulted in null pointer\n");
    return;
  }
  
  // Configure MIDI input
  libremidi_midi_configuration midi_conf;
  if (libremidi_midi_configuration_init(&midi_conf) != 0) {
    SDL_Log("ERROR: Failed to initialize MIDI configuration\n");
    libremidi_midi_in_port_free(port_clone);
    return;
  }
  
  midi_conf.version = MIDI1;
  midi_conf.in_port = port_clone;
  midi_conf.on_midi1_message.callback = on_midi1_message;
  midi_conf.on_midi1_message.context = synth;
  
  // Initialize other callbacks to prevent std::bad_function_call
  midi_conf.get_timestamp.callback = NULL;
  midi_conf.get_timestamp.context = NULL;
  midi_conf.on_error.callback = NULL;
  midi_conf.on_error.context = NULL;
  midi_conf.on_warning.callback = NULL;
  midi_conf.on_warning.context = NULL;
  
  // Set up API configuration for WebMIDI
  libremidi_api_configuration api_conf;
  if (libremidi_midi_api_configuration_init(&api_conf) != 0) {
    SDL_Log("ERROR: Failed to initialize API configuration\n");
    libremidi_midi_in_port_free(port_clone);
    return;
  }
  api_conf.configuration_type = Input;
  api_conf.api = WEBMIDI;
  api_conf.data = NULL;
  
  // Create MIDI input with bounds checking
  if (midi->device_count >= MAX_MIDI_PORTS) {
    SDL_Log("ERROR: Maximum MIDI devices reached, cannot open device\n");
    libremidi_midi_in_port_free(port_clone);
    return;
  }
  
  if (libremidi_midi_in_new(&midi_conf, &api_conf, &midi->inputs[midi->device_count]) == 0) {
    SDL_Log("  ✓ Successfully opened WebMIDI device: %.*s\n", (int)len, name);
    midi->device_count++;
    
  // Minimal delay - WebMIDI safety guard in pre.js now handles race conditions
#ifdef __EMSCRIPTEN__
  emscripten_sleep(10);
#endif
  } else {
    SDL_Log("  ✗ Failed to open WebMIDI device: %.*s\n", (int)len, name);
    libremidi_midi_in_port_free(port_clone);
  }
}

// WebMIDI callback for when devices are removed dynamically
void webmidi_input_removed(void* ctx, const libremidi_midi_in_port* port) {
  if (!ctx) {
    SDL_Log("ERROR: webmidi_input_removed called with null context\n");
    return;
  }
  
  struct Synth *synth = (struct Synth *)ctx;
  if (!synth) {
    SDL_Log("ERROR: webmidi_input_removed called with null synth\n");
    return;
  }
  
  SDL_Log("[WebMIDI] Input device removed\n");
  // Could implement device cleanup here if needed
}

// WebMIDI callback for when output devices are added (we don't use outputs, but need to prevent bad_function_call)
void webmidi_output_added(void* ctx, const libremidi_midi_out_port* port) {
  // We only handle MIDI input, so ignore output devices
  (void)ctx;
  (void)port;
}

// WebMIDI callback for when output devices are removed (we don't use outputs, but need to prevent bad_function_call)
void webmidi_output_removed(void* ctx, const libremidi_midi_out_port* port) {
  // We only handle MIDI input, so ignore output devices
  (void)ctx;
  (void)port;
}

void midi_init(Midi *midi, struct Synth *synth) {
  // Initialize MIDI structure
  memset(midi, 0, sizeof(Midi));
  
  SDL_Log("=== Initializing MIDI System ===\n");

  // Create observer configuration
  libremidi_observer_configuration observer_conf;
  int ret = libremidi_midi_observer_configuration_init(&observer_conf);
  if (ret != 0) {
    SDL_Log("ERROR: Failed to initialize observer configuration: %d\n", ret);
    return;
  }

  observer_conf.track_hardware = true;
  observer_conf.track_virtual = true;
  observer_conf.track_any = true;
  
  // Initialize all observer callbacks to prevent std::bad_function_call
  observer_conf.on_error.callback = NULL;
  observer_conf.on_error.context = NULL;
  observer_conf.on_warning.callback = NULL;
  observer_conf.on_warning.context = NULL;
  observer_conf.output_added.callback = NULL;
  observer_conf.output_added.context = NULL;
  observer_conf.output_removed.callback = NULL;
  observer_conf.output_removed.context = NULL;
  observer_conf.notify_in_constructor = false;

  // Create API configuration
  libremidi_api_configuration observer_api_conf;
  ret = libremidi_midi_api_configuration_init(&observer_api_conf);
  if (ret != 0) {
    SDL_Log("ERROR: Failed to initialize API configuration: %d\n", ret);
    return;
  }
  observer_api_conf.configuration_type = Observer;
  observer_api_conf.data = NULL;

#ifdef __EMSCRIPTEN__
  // WebMIDI implementation - More conservative initialization
  SDL_Log("Using WebMIDI API for Emscripten\n");
  
  // Set up WebMIDI-specific callbacks
  observer_conf.input_added.context = synth;
  observer_conf.input_added.callback = webmidi_input_added;
  observer_conf.input_removed.context = synth;
  observer_conf.input_removed.callback = webmidi_input_removed;
  observer_conf.output_added.context = synth;
  observer_conf.output_added.callback = webmidi_output_added;
  observer_conf.output_removed.context = synth;
  observer_conf.output_removed.callback = webmidi_output_removed;
  
  observer_api_conf.api = WEBMIDI;
  
  // Create observer
  libremidi_midi_observer_handle* observer = NULL;
  ret = libremidi_midi_observer_new(&observer_conf, &observer_api_conf, &observer);
  if (ret != 0) {
    SDL_Log("ERROR: Failed to create WebMIDI observer: %d\n", ret);
    return;
  }
  
  midi->observer = observer;
  midi->enabled = 1; // Ready to receive devices
  midi->device_count = 0; // No devices yet
  
  SDL_Log("WebMIDI observer created - waiting for user interaction to enable MIDI access\n");
  SDL_Log("Once enabled, MIDI devices will be automatically connected\n");
  
  // Minimal delay - WebMIDI safety guard in pre.js now handles race conditions
  emscripten_sleep(10);
  
#else
  // Native implementation
  SDL_Log("Using native MIDI API\n");
  
  // Set up enumeration callback
  observer_conf.input_added.context = midi;
  observer_conf.input_added.callback = on_input_port_found;

#if defined(__linux__)
  observer_api_conf.api = ALSA_SEQ;
  SDL_Log("Using ALSA Sequencer API\n");
#elif defined(_WIN32)
  observer_api_conf.api = WINDOWS_MM;
  SDL_Log("Using Windows Multimedia API\n");
#elif defined(__APPLE__)
  observer_api_conf.api = COREMIDI;
  SDL_Log("Using CoreMIDI API\n");
#else
  observer_api_conf.api = UNSPECIFIED;
  SDL_Log("Using unspecified MIDI API\n");
#endif

  // Create observer
  libremidi_midi_observer_handle* observer = NULL;
  ret = libremidi_midi_observer_new(&observer_conf, &observer_api_conf, &observer);
  if (ret != 0) {
    SDL_Log("ERROR: Failed to create MIDI observer: %d\n", ret);
    return;
  }

  // Enumerate ports
  midi_enumerated_ports enumerated = {0};
  ret = libremidi_midi_observer_enumerate_input_ports(observer, &enumerated, on_input_port_found);
  if (ret != 0) {
    SDL_Log("ERROR: Failed to enumerate MIDI input ports: %d\n", ret);
    libremidi_midi_observer_free(observer);
    return;
  }
  
  SDL_Log("Found %d MIDI input device(s)\n", enumerated.count);

  // Open each enumerated port
  int successfully_opened = 0;
  for (int i = 0; i < enumerated.count && i < MAX_MIDI_PORTS; i++) {
    const char* name = NULL;
    size_t len = 0;
    libremidi_midi_in_port_name(enumerated.ports[i], &name, &len);
    
    SDL_Log("Opening MIDI device %d: %.*s\n", i, (int)len, name);
    
    // Configure MIDI input
    libremidi_midi_configuration midi_conf;
    if (libremidi_midi_configuration_init(&midi_conf) != 0) {
      SDL_Log("  ✗ Failed to initialize MIDI configuration\n");
      continue;
    }
    
    midi_conf.version = MIDI1;
    midi_conf.in_port = enumerated.ports[i];
    midi_conf.on_midi1_message.callback = on_midi1_message;
    midi_conf.on_midi1_message.context = synth;
    
    // Initialize other callbacks to prevent std::bad_function_call
    midi_conf.get_timestamp.callback = NULL;
    midi_conf.get_timestamp.context = NULL;
    midi_conf.on_error.callback = NULL;
    midi_conf.on_error.context = NULL;
    midi_conf.on_warning.callback = NULL;
    midi_conf.on_warning.context = NULL;
    
    // Set up API configuration for input
    libremidi_api_configuration api_conf;
    if (libremidi_midi_api_configuration_init(&api_conf) != 0) {
      SDL_Log("  ✗ Failed to initialize API configuration\n");
      continue;
    }
    api_conf.configuration_type = Input;
    api_conf.data = NULL;
    
#if defined(__linux__)
    api_conf.api = ALSA_SEQ;
#elif defined(_WIN32)
    api_conf.api = WINDOWS_MM;
#elif defined(__APPLE__)
    api_conf.api = COREMIDI;
#else
    api_conf.api = UNSPECIFIED;
#endif

    // Create MIDI input
    if (libremidi_midi_in_new(&midi_conf, &api_conf, &midi->inputs[successfully_opened]) == 0) {
      SDL_Log("  ✓ Successfully opened MIDI device: %.*s\n", (int)len, name);
      successfully_opened++;
      
      // Workaround: Add small delay to allow MIDI system to stabilize
      // This helps prevent potential race conditions in some MIDI backends
#ifdef __EMSCRIPTEN__
      emscripten_sleep(5);
#endif
    } else {
      SDL_Log("  ✗ Failed to open MIDI device: %.*s\n", (int)len, name);
    }
  }
  
  // Clean up
  for (int i = 0; i < enumerated.count; i++) {
    if (enumerated.ports[i]) {
      libremidi_midi_in_port_free(enumerated.ports[i]);
    }
  }
  libremidi_midi_observer_free(observer);
  
  midi->device_count = successfully_opened;
  midi->enabled = (successfully_opened > 0) ? 1 : 0;

  // Final summary
  SDL_Log("MIDI initialization complete:\n");
  SDL_Log("  - MIDI system enabled: %s\n", midi->enabled ? "YES" : "NO");
  SDL_Log("  - Active MIDI devices: %d\n", midi->device_count);
  if (midi->enabled) {
    SDL_Log("  - Ready to receive MIDI input\n");
  } else {
    SDL_Log("  - No MIDI devices available\n");
  }
#endif

  midi->last_cc = -1;
  midi->last_cc_value = 0;
}

void midi_shutdown(Midi *midi) {
  // First, set a flag to prevent any new MIDI processing
  midi->enabled = 0;
  
  // Add a longer delay to allow any in-progress callbacks to complete
  SDL_Delay(100);
  
  // Temporarily skip freeing observer and MIDI inputs to avoid segfault
  // TODO: Fix proper shutdown when libremidi library is stable
  if (midi->observer) {
    // libremidi_midi_observer_free(midi->observer);  // Commented out to avoid segfault
    midi->observer = NULL;
  }
  
  // Add another delay after skipping observer cleanup
  SDL_Delay(50);
  
  // Temporarily skip freeing MIDI inputs to avoid segfault
  for (int i = 0; i < midi->device_count; i++) {
    if (midi->inputs[i]) {
      // libremidi_midi_in_free(midi->inputs[i]);  // Commented out to avoid segfault
      midi->inputs[i] = NULL;
    }
  }
  
  midi->device_count = 0;
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
}
