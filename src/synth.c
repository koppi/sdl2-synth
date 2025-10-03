#include "synth.h"
#include "oscilloscope.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Startup melody: "Twinkle Twinkle Little Star" in C major
static MelodyNote startup_melody_data[] = {
    {60, 0.5f, 0.6f}, // C4 - Reduced velocity from 0.8f to 0.6f
    {60, 0.5f, 0.6f}, // C4
    {67, 0.5f, 0.6f}, // G4
    {67, 0.5f, 0.6f}, // G4
    {69, 0.5f, 0.6f}, // A4
    {69, 0.5f, 0.6f}, // A4
    {67, 1.0f, 0.6f}, // G4
    {65, 0.5f, 0.6f}, // F4
    {65, 0.5f, 0.6f}, // F4
    {64, 0.5f, 0.6f}, // E4
    {64, 0.5f, 0.6f}, // E4
    {62, 0.5f, 0.6f}, // D4
    {62, 0.5f, 0.6f}, // D4
    {60, 1.0f, 0.6f}, // C4
};

// Bach-inspired chord progression: i - iv - V - i in C minor (like Bach's inventions)
static ChordProgression bach_progression_data[] = {
    // Cm (C minor) - i chord
    {{48, 51, 55, 60}, 4, 4.0f}, // C3, Eb3, G3, C4
    
    // Fm (F minor) - iv chord  
    {{53, 56, 60, 65}, 4, 4.0f}, // F3, Ab3, C4, F4
    
    // G (G major) - V chord
    {{55, 59, 62, 67}, 4, 4.0f}, // G3, B3, D4, G4
    
    // Cm (C minor) - i chord (resolution)
    {{48, 51, 55, 60}, 4, 4.0f}, // C3, Eb3, G3, C4
    
    // Am7b5 (A half-diminished) - ii chord in minor
    {{57, 60, 63, 67}, 4, 4.0f}, // A3, C4, Eb4, G4
    
    // G7 (G dominant 7th) - V7 chord
    {{55, 59, 62, 65}, 4, 4.0f}, // G3, B3, D4, F4
    
    // Cm (C minor) - final resolution
    {{48, 51, 55, 60}, 4, 6.0f}, // C3, Eb3, G3, C4 (longer final chord)
};

static int order_compare(const void *a, const void *b) {
  return (*(int *)a) - (*(int *)b);
}

static int arp_last_played_note = -1;

void synth_update_arpeggiator(Synth *synth, int frames) {
    Arpeggiator *arp = &synth->arp;
    if (!arp->enabled || arp->held_count == 0) {
        if (arp_last_played_note != -1) {
            synth_note_off(synth, arp_last_played_note);
            arp_last_played_note = -1;
        }
        return;
    }

    float dt = (float)frames / synth->sample_rate;
    float step_time = 60.0f / arp->tempo / 4.0f; // 16th notes

    arp->step_phase += dt;

    if (arp->step_phase >= step_time) {
        arp->step_phase -= step_time;

        if (arp_last_played_note != -1) {
            synth_note_off(synth, arp_last_played_note);
            arp_last_played_note = -1;
        }

        int step_index = arp->step % ARP_PATTERN_LEN;
        if (arp->pattern[step_index]) {
            int note_index = 0;
            int notes[16];
            memcpy(notes, arp->held_notes, sizeof(int) * arp->held_count);

            switch (arp->mode) {
                case ARP_UP:
                    qsort(notes, arp->held_count, sizeof(int), order_compare);
                    note_index = arp->step % arp->held_count;
                    break;
                case ARP_DOWN:
                    qsort(notes, arp->held_count, sizeof(int), order_compare);
                    note_index = arp->held_count - 1 - (arp->step % arp->held_count);
                    break;
                case ARP_ORDER:
                    note_index = arp->step % arp->held_count;
                    break;
                case ARP_RANDOM:
                    note_index = rand() % arp->held_count;
                    break;
            }
            
            if (arp->held_count > 0) {
                int note_to_play = notes[note_index];
                synth_note_on(synth, note_to_play, 0.8f);
                arp_last_played_note = note_to_play;
            }
        }
        
        arp->step++;
    }
}

int synth_init(Synth *synth, int samplerate, int buffer_size, int voices) {
  memset(synth, 0, sizeof(Synth));
  synth->max_voices = voices;
  synth->sample_rate = samplerate;
  
  // Initialize melody system
  synth->startup_melody = startup_melody_data;
  synth->melody_length = sizeof(startup_melody_data) / sizeof(MelodyNote);
  synth->melody_index = 0;
  synth->melody_timer = 0.0f;
  synth->melody_playing = 0;
  synth->melody_finished = 0;
  synth->pause_timer = 0.0f;
  
  // Initialize Bach progression system
  synth->bach_progression = bach_progression_data;
  synth->progression_length = sizeof(bach_progression_data) / sizeof(ChordProgression);
  synth->progression_index = 0;
  synth->progression_timer = 0.0f;
  synth->progression_playing = 0;
  synth->current_chord_size = 0;
  
  mixer_init(&synth->mixer);
  mixer_set_sample_rate(&synth->mixer, samplerate);
  fx_init(&synth->fx, samplerate);
  arpeggiator_init(&synth->arp);
  
  // Configure arpeggiator for Bach progression
  synth->arp.enabled = 0; // Will be enabled when progression starts
  synth->arp.tempo = 120.0f;
  synth->arp.mode = ARP_UP;
  
  midi_init(&synth->midi, synth);
  for (int i = 0; i < 4; ++i)
    osc_init(&synth->osc[i], samplerate);
  
  // Set specific oscillator gains as requested
  synth->osc[0].gain = 1.0f;   // OSC1: gain = 1.0
  synth->osc[1].gain = 0.0f;   // OSC2: gain = 0.0
  synth->osc[2].gain = 0.0f;   // OSC3: gain = 0.0
  synth->osc[3].gain = 0.0f;   // OSC4: gain = 0.0
  
  for (int v = 0; v < voices; ++v)
    voice_init(&synth->voices[v], samplerate);
  synth->timestamp_counter = 0;
  return 1;
}

void synth_shutdown(Synth *synth) { midi_shutdown(&synth->midi); }

void synth_audio_callback(void *userdata, Uint8 *stream, int len) {
  Synth *synth = (Synth *)userdata;
  const int frames = len / (sizeof(float) * 2);
  float *out = (float *)stream;
  memset(out, 0, sizeof(float) * frames * 2);

  // Update melody system
  synth_update_melody(synth, frames);
  
  // Update Bach progression system
  synth_update_bach_progression(synth, frames);

  synth_update_arpeggiator(synth, frames);

  float *vbuf = (float *)calloc(frames * 2, sizeof(float));
  if (!vbuf)
    return; // or handle error
  for (int v = 0; v < synth->max_voices; ++v) {
    if (!synth->voices[v].active)
      continue;
    memset(vbuf, 0, sizeof(float) * frames * 2);
    voice_render(&synth->voices[v], synth->osc, vbuf, frames);
    for (int i = 0; i < frames * 2; ++i)
      out[i] += vbuf[i] * synth->voices[v].velocity;
  }
  free(vbuf);

  mixer_apply(&synth->mixer, out, frames);
  fx_process(&synth->fx, out, frames);

  static clock_t last = 0;
  clock_t now = clock();
  synth->cpu_usage = 100.0f * ((float)(now - last) / (float)CLOCKS_PER_SEC) /
                     (frames / 48000.0f);
  last = now;

  // Feed oscilloscope with proper audio samples
  static int debug_counter = 0;
  float max_sample = 0.0f;
  
  for (int n = 0; n < frames; ++n) {
    float mixed_sample = 0.5f * (out[n * 2 + 0] + out[n * 2 + 1]);
    // Apply gain to make waveform visible (compensate for lower audio levels)
    mixed_sample *= 8.0f;  // Increased from 2.0f to 8.0f to compensate for 0.25 audio level
	oscilloscope_feed(mixed_sample);
    oscilloscope_update_fft(mixed_sample);
    
    // Track max amplitude for debugging
    float abs_sample = fabsf(mixed_sample);
    if (abs_sample > max_sample) max_sample = abs_sample;
  }
  
  // Debug output every 2 seconds
  debug_counter++;
  if (debug_counter >= 48000 * 2) {  // Roughly every 2 seconds at 48kHz
    printf("Oscilloscope: max amplitude = %.3f, voices = %d\n", 
           max_sample, synth_active_voices(synth));
    debug_counter = 0;
  }
}

int synth_active_voices(const Synth *synth) {
  int n = 0;
  for (int i = 0; i < synth->max_voices; ++i)
    if (synth->voices[i].active)
      ++n;
  return n;
}

float synth_cpu_usage(const Synth *synth) { return synth->cpu_usage; }

void synth_handle_cc(Synth *synth, int cc, int value) {
  midi_map_cc_to_param(synth, cc, value);
}

void synth_set_param(Synth *synth, const char *param, float value) {
  if (strncmp(param, "osc", 3) == 0) {
    int i = param[3] - '1';
    if (i >= 0 && i < 4)
      osc_set_param(&synth->osc[i], param + 5, value);
  } else if (strncmp(param, "fx.", 3) == 0) {
    fx_set_param(&synth->fx, param + 3, value);
  } else if (strncmp(param, "mixer.", 6) == 0) {
    mixer_set_param(&synth->mixer, param + 6, value);
  } else if (strncmp(param, "arp.", 4) == 0) {
    arpeggiator_set_param(&synth->arp, param + 4, value);
  }
}

void synth_note_on(Synth *synth, int note, float velocity) {
  synth->timestamp_counter++; // Increment timestamp for new note

  // 1. If note already playing, retrigger that voice
  for (int v = 0; v < synth->max_voices; ++v) {
    if (synth->voices[v].active && (int)synth->voices[v].note == note) {
      voice_on(&synth->voices[v], note, velocity, synth->timestamp_counter);
      return;
    }
  }

  // 2. Otherwise, find a free voice and use it
  for (int v = 0; v < synth->max_voices; ++v) {
    if (!synth->voices[v].active) {
      voice_on(&synth->voices[v], note, velocity, synth->timestamp_counter);
      return;
    }
  }

  // 3. If none free, steal the oldest note
  unsigned long long min_timestamp = -1; // Max value for unsigned long long
  int voice_to_steal_idx = -1;

  for (int v = 0; v < synth->max_voices; ++v) {
    // Only consider active voices for stealing
    if (synth->voices[v].active) {
      if (synth->voices[v].timestamp < min_timestamp) {
        min_timestamp = synth->voices[v].timestamp;
        voice_to_steal_idx = v;
      }
    }
  }

  // If a voice was found to steal (should always be one if max_voices > 0)
  if (voice_to_steal_idx != -1) {
    voice_on(&synth->voices[voice_to_steal_idx], note, velocity,
             synth->timestamp_counter);
  }
}

void synth_note_off(Synth *synth, int note) {
  for (int v = 0; v < synth->max_voices; ++v) {
    if (synth->voices[v].active && (int)synth->voices[v].note == note) {
      voice_off(&synth->voices[v]);
      return;
    }
  }
}

void synth_start_melody(Synth *synth) {
  synth->melody_playing = 1;
  synth->melody_index = 0;
  synth->melody_timer = 0.0f;
}

void synth_update_melody(Synth *synth, int frames) {
  if (!synth->melody_playing && !synth->melody_finished) {
    return;
  }
  
  float dt = (float)frames / synth->sample_rate;
  
  // Handle transition pause after melody finishes
  if (synth->melody_finished && !synth->progression_playing) {
    synth->pause_timer += dt;
    if (synth->pause_timer >= 1.0f) { // 1 second pause
      synth_start_bach_progression(synth);
      synth->melody_finished = 0; // Reset state
      synth->pause_timer = 0.0f;
    }
    return;
  }
  
  if (!synth->melody_playing || synth->melody_index >= synth->melody_length) {
    return;
  }
  
  synth->melody_timer += dt;
  
  MelodyNote *current_note = &synth->startup_melody[synth->melody_index];
  
  // Play the current note if we just started or moved to a new note
  static int last_note_played = -1;
  if (last_note_played != synth->melody_index) {
    synth_note_on(synth, current_note->note, current_note->velocity);
    last_note_played = synth->melody_index;
  }
  
  // Check if it's time to move to the next note
  if (synth->melody_timer >= current_note->duration) {
    // Stop the current note
    synth_note_off(synth, current_note->note);
    
    // Move to next note
    synth->melody_index++;
    synth->melody_timer = 0.0f;
    
    // Check if melody is finished
    if (synth->melody_index >= synth->melody_length) {
      synth->melody_playing = 0;
      synth->melody_finished = 1; // Mark as finished to start pause
      printf("Startup melody finished, starting pause before Bach progression\n");
    }
  }
}

void synth_start_bach_progression(Synth *synth) {
  synth->progression_playing = 1;
  synth->progression_index = 0;
  synth->progression_timer = 0.0f;
  synth->arp.enabled = 1; // Enable arpeggiator for progression
  printf("Starting Bach-inspired progression with arpeggiator\n");
}

void synth_update_bach_progression(Synth *synth, int frames) {
  if (!synth->progression_playing || synth->progression_index >= synth->progression_length) {
    return;
  }
  
  float dt = (float)frames / synth->sample_rate;
  synth->progression_timer += dt;
  
  ChordProgression *current_chord = &synth->bach_progression[synth->progression_index];
  
  // Update arpeggiator with current chord notes
  static int last_chord_set = -1;
  if (last_chord_set != synth->progression_index) {
    // Clear old chord from arpeggiator
    synth->arp.held_count = 0;
    
    // Set new chord in arpeggiator
    synth->current_chord_size = current_chord->chord_size;
    for (int i = 0; i < current_chord->chord_size; i++) {
      synth->current_chord_notes[i] = current_chord->chord[i];
      arpeggiator_note_on(&synth->arp, current_chord->chord[i]);
    }
    
    last_chord_set = synth->progression_index;
    printf("Playing chord %d with %d notes: ", synth->progression_index + 1, current_chord->chord_size);
    for (int i = 0; i < current_chord->chord_size; i++) {
      printf("%d ", current_chord->chord[i]);
    }
    printf("(held_count: %d)\n", synth->arp.held_count);
  }
  
  // Simple manual arpeggiation instead of using the complex arpeggiator
  static float arp_timer = 0.0f;
  static int arp_note_index = 0;
  static int last_played_note = -1;
  
  arp_timer += dt;
  float note_duration = 0.3f; // 300ms per arpeggiated note
  
  if (arp_timer >= note_duration && current_chord->chord_size > 0) {
    // Stop previous note
    if (last_played_note >= 0) {
      synth_note_off(synth, last_played_note);
    }
    
    // Play next note in chord
    int note_to_play = current_chord->chord[arp_note_index];
    synth_note_on(synth, note_to_play, 0.6f);  // Reduced from 0.8f to 0.6f to match new amplitude levels
    last_played_note = note_to_play;
    
    printf("Playing arp note: %d (index %d)\n", note_to_play, arp_note_index);
    
    // Move to next note in chord
    arp_note_index = (arp_note_index + 1) % current_chord->chord_size;
    arp_timer = 0.0f;
  }
  
  // Check if it's time to move to the next chord
  if (synth->progression_timer >= current_chord->duration) {
    // Stop current note before changing chord
    if (last_played_note >= 0) {
      synth_note_off(synth, last_played_note);
      last_played_note = -1;
    }
    
    synth->progression_index++;
    synth->progression_timer = 0.0f;
    arp_timer = 0.0f;
    arp_note_index = 0;
    
    // Check if progression is finished
    if (synth->progression_index >= synth->progression_length) {
      synth->progression_playing = 0;
      synth->arp.enabled = 0; // Disable arpeggiator
      synth->arp.held_count = 0;
      synth->current_chord_size = 0;
      printf("Bach progression completed\n");
    }
  }
}
