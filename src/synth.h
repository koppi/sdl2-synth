#pragma once
#include "arpeggiator.h"
#include "fx.h"
#include "midi.h"
#include "mixer.h"
#include "osc.h"
#include "voice.h"
#include <SDL.h>

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
  Oscillator osc[4];
  Mixer mixer;
  FX fx;
  Voice voices[64];
  int max_voices;
  Arpeggiator arp;
  Midi midi;
  float cpu_usage;
  unsigned long long timestamp_counter;
  
  // Startup melody system
  MelodyNote *startup_melody;
  int melody_length;
  int melody_index;
  float melody_timer;
  int melody_playing;
  float sample_rate;
  
  // Bach progression system
  ChordProgression *bach_progression;
  int progression_length;
  int progression_index;
  float progression_timer;
  int progression_playing;
  int current_chord_notes[4];
  int current_chord_size;
  
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
void synth_start_melody(Synth *synth);
void synth_update_melody(Synth *synth, int frames);
void synth_start_bach_progression(Synth *synth);
void synth_update_bach_progression(Synth *synth, int frames);
