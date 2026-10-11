#include "midi.h"

#include "synth.h"
#include "arpeggiator.h"

#include <SDL2/SDL.h>

// Include libremidi headers - they appear to be found by the build system
#include <libremidi/libremidi-c.h>
#include <libremidi/api-c.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        synth_midi_note_off(synth, note);
        break;
        
      case 0x90: // Note On (with velocity)
        synth_midi_note_on(synth, note, vel / 127.0f);
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

