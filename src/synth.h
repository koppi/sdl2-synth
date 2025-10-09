#pragma once
#include "arpeggiator.h"
#include "fx.h"
#include "midi.h"
#include "mixer.h"
#include "osc.h"
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
  Oscillator osc[6]; // osc[0-3] for main, osc[4] for bass, osc[5] for percussion
  Mixer mixer;
  FX fx;
  Voice voices[64];
  int max_voices;
  Arpeggiator arp;
  Midi midi;
  float cpu_usage;
  unsigned long long timestamp_counter;
  
  float sample_rate;
  
  // Transition state
  int melody_finished;
  float pause_timer;
} Synth;

int synth_init(Synth *synth, int samplerate, int buffer_size, int voices);
void synth_shutdown(Synth *synth);
void synth_audio_callback(void *userdata, Uint8 *stream, int len);
int synth_active_voices(const Synth *synth);
float synth_cpu_usage(const Synth *synth);
void synth_handle_cc(Synth *synth, int cc, int value);
void synth_set_param(Synth *synth, const char *param, float value);

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
