#pragma once
#include "arpeggiator.h"
#include "adsr.h"
#include "fx.h"
#include "lfo.h"
#include "mixer.h"
#include "midi.h"
#include "osc.h"
#include "ring_modulator.h"
#include "voice.h"
#include <SDL2/SDL.h>

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
  Voice voices[64];
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
} Synth;

int synth_init(Synth *synth, int samplerate, int buffer_size, int voices);
void synth_shutdown(Synth *synth);
void synth_audio_callback(void *userdata, Uint8 *stream, int len);
int synth_active_voices(const Synth *synth);
float synth_cpu_usage(const Synth *synth);
void synth_set_bpm(Synth *synth, float bpm);
void synth_handle_cc(Synth *synth, int cc, int value);
#ifdef __cplusplus
extern "C" {
  void synth_set_param(Synth *synth, const char *param, float value);
}
#endif

void synth_note_on(Synth *synth, int note, float velocity);
void synth_note_off(Synth *synth, int note);
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
#ifdef __cplusplus
extern "C" {
#endif
void synth_set_param(Synth *synth, const char *param, float value);

void synth_randomize_parameters(Synth *synth);
void synth_randomize_oscillator(Synth *synth, int osc_index);
#ifdef __cplusplus
}
#endif
