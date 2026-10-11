#pragma once
#include "arpeggiator.h"
#include "adsr.h"
#include "fx.h"
#include "lfo.h"
#include "mixer.h"
#include "midi.h"
#include "osc.h"
#include "ring_modulator.h"
#include "scope.h"
#include "voice.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The engine renders in fixed-size chunks so envelopes, LFOs and the
// arpeggiator behave identically no matter how large the host's audio buffers
// are.
#define SYNTH_CHUNK_FRAMES 64
#define SYNTH_MAX_VOICES 64

typedef struct {
  int note;
  float duration;
  float velocity;
} MelodyNote;

typedef struct {
  int chord[4];  // Up to 4 notes per chord
  int chord_size;
  float duration;
} ChordProgression;

typedef struct Synth {
  Oscillator osc[4]; // osc[0-3] for main oscillators
  LFO lfos[3]; // lfos[0] for pitch, lfos[1] for volume, lfos[2] for filter
  Mixer mixer;
  FX fx;
  RingModulator ring_mod;
  Voice voices[SYNTH_MAX_VOICES];
  int max_voices;
  Arpeggiator arp;
  Midi midi;
  float cpu_usage;
  unsigned long long timestamp_counter;

  float sample_rate;
  AdsrEnvelope adsr; // Global ADSR settings

  // Transition state
  int melody_finished;
  float pause_timer;

  // Global pitch offset in semitones (pitch wheel). Applied to every voice.
  float pitch_bend;

  // Plug-in only: when set, the host transport tempo drives the arpeggiator
  // and the tempo-synced delay instead of their own tempo knobs.
  int host_sync;

  // Optional waveform tap (owned by the caller) for the oscilloscope display.
  ScopeBuffer *scope;

  // Measure CPU load across audio callbacks (standalone app only).
  int measure_cpu;
  unsigned long long cpu_last_clock;

  // Note currently held by the arpeggiator's monophonic path (-1 = none).
  int arp_last_played_note;

  // Scratch space for rendering one chunk of voices.
  float voice_scratch[SYNTH_CHUNK_FRAMES * 2];
} Synth;

// Initialises the engine with deterministic defaults. Performs no I/O, touches
// no global state and spawns no threads, so several instances can coexist
// (needed by plug-in hosts). `samplerate` is in Hz, `voices` is clamped to
// SYNTH_MAX_VOICES. Release the instance with synth_core_shutdown().
int synth_core_init(Synth *synth, int samplerate, int voices);
void synth_core_shutdown(Synth *synth);

// Silences all voices and clears delay/reverb/filter tails and arpeggiator
// state. Parameter values are left untouched.
void synth_reset(Synth *synth);

// Renders `frames` stereo frames (interleaved L/R floats), overwriting
// `stereo`. Real-time safe: does not allocate.
void synth_render(Synth *synth, float *stereo, int frames);

// SDL_AudioCallback compatible wrapper around synth_render() for the
// standalone app. Expects an AUDIO_F32 stereo stream.
void synth_audio_callback(void *userdata, uint8_t *stream, int len);

int synth_active_voices(const Synth *synth);
float synth_cpu_usage(const Synth *synth);
void synth_set_bpm(Synth *synth, float bpm);
void synth_handle_cc(Synth *synth, int cc, int value);
void synth_set_param(Synth *synth, const char *param, float value);

void synth_note_on(Synth *synth, int note, float velocity);
void synth_note_off(Synth *synth, int note);

// MIDI-style note input: routed through the arpeggiator when it is enabled,
// otherwise played directly. A note-on with zero velocity is a note-off.
void synth_midi_note_on(Synth *synth, int note, float velocity);
void synth_midi_note_off(Synth *synth, int note);
void synth_all_notes_off(Synth *synth);

void synth_play_startup_melody(Synth *synth);
void synth_update_startup_melody(Synth *synth, int frames);
void synth_start_melody(Synth *synth);
void synth_update_melody(Synth *synth, int frames);
void synth_start_bach_progression(Synth *synth);
void synth_update_bach_progression(Synth *synth, int frames);
char* synth_save_preset_json(const Synth *synth);
void synth_save_default_config(const Synth *synth);
void synth_load_default_config(Synth *synth);
void synth_load_preset_json(Synth *synth, const char *json_string);

void synth_randomize_parameters(Synth *synth);
void synth_randomize_oscillator(Synth *synth, int osc_index);

#ifdef __cplusplus
}
#endif
