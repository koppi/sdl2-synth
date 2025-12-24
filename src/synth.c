#include "synth.h"
#include "midi.h"
#include "oscilloscope.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

static int order_compare(const void *a, const void *b) {
  return (*(int *)a) - (*(int *)b);
}

static int arp_last_played_note = -1;

// Define a simple startup chord progression
static MelodyNote startup_melody[] = {
    // C Major Chord
    {60, 0.7f, 0.8f}, // C4
    {64, 0.7f, 0.8f}, // E4
    {67, 0.7f, 0.8f}, // G4
    
    // G Major Chord
    {67, 0.7f, 0.8f}, // G4
    {71, 0.7f, 0.8f}, // B4
    {74, 0.7f, 0.8f}, // D5

    // A Minor Chord
    {69, 0.7f, 0.8f}, // A4
    {72, 0.7f, 0.8f}, // C5
    {76, 0.7f, 0.8f}, // E5

    // F Major Chord
    {65, 0.7f, 0.8f}, // F4
    {69, 0.7f, 0.8f}, // A4
    {72, 0.7f, 0.8f}, // C5

    {-1, 0.0f, 0.0f}  // End marker
};
static int current_melody_note_idx = 0;
static float melody_time_elapsed = 0.0f;
static int melody_playing = 0;

typedef struct {
    int note;
    float remaining_duration;
    int active;
} ActiveMelodyNote;
#define MAX_MELODY_POLYPHONY 8 
static ActiveMelodyNote active_melody_notes[MAX_MELODY_POLYPHONY];

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

    // Update active arpeggiated notes duration every frame
    for (int i = 0; i < 16; ++i) {
        if (arp->active_arpeggiated_notes[i].active) {
            arp->active_arpeggiated_notes[i].remaining_duration -= dt;
            if (arp->active_arpeggiated_notes[i].remaining_duration <= 0) {
                synth_note_off(synth, arp->active_arpeggiated_notes[i].note);
                arp->active_arpeggiated_notes[i].active = 0;
            }
        }
    }

    if (arp->step_phase >= step_time) {
        arp->step_phase -= step_time;

        // The arpeggiator_step function now handles note_on/off directly
        arpeggiator_step(arp, synth->sample_rate, synth);
        
        arp->step++;
    }
}

void synth_play_startup_melody(Synth *synth) {
    current_melody_note_idx = 0;
    melody_time_elapsed = 0.0f;
    melody_playing = 1;
}

void synth_update_startup_melody(Synth *synth, int frames) {
    if (!melody_playing) return;

    float dt = (float)frames / synth->sample_rate;

    // Update active notes and turn off expired ones
    for (int i = 0; i < MAX_MELODY_POLYPHONY; ++i) {
        if (active_melody_notes[i].active) {
            active_melody_notes[i].remaining_duration -= dt;
            if (active_melody_notes[i].remaining_duration <= 0) {
                synth_note_off(synth, active_melody_notes[i].note);
                active_melody_notes[i].active = 0;
            }
        }
    }

    melody_time_elapsed += dt;

    if (startup_melody[current_melody_note_idx].note == -1) { // End of melody
        melody_playing = 0;
        // Turn off any remaining active notes
        for (int i = 0; i < MAX_MELODY_POLYPHONY; ++i) {
            if (active_melody_notes[i].active) {
                synth_note_off(synth, active_melody_notes[i].note);
                active_melody_notes[i].active = 0;
            }
        }
        return;
    }

    if (melody_time_elapsed >= startup_melody[current_melody_note_idx].duration) {
        // Find a free slot for the new note
        int free_slot = -1;
        for (int i = 0; i < MAX_MELODY_POLYPHONY; ++i) {
            if (!active_melody_notes[i].active) {
                free_slot = i;
                break;
            }
        }

        if (free_slot != -1) {
            active_melody_notes[free_slot].note = startup_melody[current_melody_note_idx].note;
            active_melody_notes[free_slot].remaining_duration = startup_melody[current_melody_note_idx].duration;
            active_melody_notes[free_slot].active = 1;
            synth_note_on(synth, startup_melody[current_melody_note_idx].note, startup_melody[current_melody_note_idx].velocity);
        } else {
            // No free slot, voice stealing will handle it if synth has enough voices
            synth_note_on(synth, startup_melody[current_melody_note_idx].note, startup_melody[current_melody_note_idx].velocity);
        }
        
        melody_time_elapsed = 0.0f;
        current_melody_note_idx++;
    }
}

int synth_init(Synth *synth, int samplerate, int buffer_size, int voices) {
  memset(synth, 0, sizeof(Synth));
  synth->max_voices = voices;
  synth->sample_rate = samplerate;
  
  srand(time(NULL)); // Seed random number generator

  mixer_init(&synth->mixer);
  mixer_set_sample_rate(&synth->mixer, samplerate);
  synth_set_param(synth, "mixer.master", 1.0f);
  fx_init(&synth->fx, samplerate);
  synth_set_param(synth, "fx.delay.mix", 0.0f); // Set delay mix to 0.0
  synth_set_param(synth, "fx.reverb.mix", 0.05f); // Enable reverb with a mix of 0.05
  ring_mod_init(&synth->ring_mod, samplerate);
  // Set default ring modulator settings like Roland System 100
  synth_set_param(synth, "ring_mod.frequency", 20.0f);  // Low frequency for metallic character
  synth_set_param(synth, "ring_mod.mix", 0.0f);         // Disabled by default
  synth_set_param(synth, "ring_mod.enabled", 0);        // Disabled by default
  arpeggiator_init(&synth->arp);
  

  synth->arp.enabled = 1; // Will be enabled when progression starts
  synth->arp.tempo = 120.0f;
  synth->arp.mode = ARP_UP;
  
  midi_init(&synth->midi, synth);
  
  // Initialize LFOs
  for (int i = 0; i < 3; ++i) {
    lfo_init(&synth->lfos[i], samplerate);
  }
  // Set default LFO targets
  synth->lfos[0].target = LFO_TARGET_FREQUENCY;
  synth->lfos[1].target = LFO_TARGET_AMPLITUDE;
  synth->lfos[2].target = LFO_TARGET_FILTER;
  // Set default LFO parameters
  synth_set_param(synth, "lfo1.frequency", 2.0f);
  synth_set_param(synth, "lfo1.depth", 0.1f);
  synth_set_param(synth, "lfo1.waveform", LFO_SINE);
  synth_set_param(synth, "lfo1.sync", 1.0f); // LFO_SYNC_RETRIGGER
  synth_set_param(synth, "lfo2.sync", 1.0f); // LFO_SYNC_RETRIGGER
  synth_set_param(synth, "lfo3.sync", 1.0f); // LFO_SYNC_RETRIGGER
  
  // synth_play_startup_melody(synth); // Start the melody at startup
  
  char param_name[16];
  for (int i = 0; i < 4; ++i) { // Loop for all 4 oscillators
    osc_init(&synth->osc[i], samplerate);

    // Default parameters for main oscillators (0-3)
    if (i < 4) {
      // Randomize pitch (-0.5 to 0.5 semitones)
      float random_pitch = -0.5f + ((float)rand() / (float)RAND_MAX) * (1.0f);
      snprintf(param_name, sizeof(param_name), "osc%d.pitch", i + 1);
      synth_set_param(synth, param_name, random_pitch);

      // Randomize detune (-0.05 to 0.05)
      float random_detune = -0.05f + ((float)rand() / (float)RAND_MAX) * (0.1f);
      snprintf(param_name, sizeof(param_name), "osc%d.detune", i + 1);
      synth_set_param(synth, param_name, random_detune);

      // Randomize gain (0.3 to 0.7)
      float random_gain = 0.3f + ((float)rand() / (float)RAND_MAX) * (0.4f);
      snprintf(param_name, sizeof(param_name), "osc%d.gain", i + 1);
      synth_set_param(synth, param_name, random_gain);
      char waveform_param_name[16];
      snprintf(waveform_param_name, sizeof(waveform_param_name), "osc%d.waveform", i + 1);
      synth_set_param(synth, waveform_param_name, 0.0f); // Default waveform to sine
    }
  }
  
  // Specific default parameters for Bass (osc[4])
  
  
  synth_set_param(synth, "osc1.waveform", 0.0f);
  synth_set_param(synth, "osc2.waveform", 0.0f);
  synth_set_param(synth, "osc3.waveform", 0.0f);
  synth_set_param(synth, "osc4.waveform", 0.0f);
  
  // Initialize global ADSR
  adsr_init(&synth->adsr, samplerate);
  
  for (int v = 0; v < voices; ++v)
    voice_init(&synth->voices[v], samplerate);
  synth->timestamp_counter = 0;
  
  // Initialize active_melody_notes
  for (int i = 0; i < MAX_MELODY_POLYPHONY; ++i) {
      active_melody_notes[i].active = 0;
  }

#ifndef __EMSCRIPTEN__
  // Load default config at startup
  synth_load_default_config(synth);
#endif
  
  return 1;
}

void synth_shutdown(Synth *synth) { 
#ifndef __EMSCRIPTEN__
  // Save current settings to default config before shutdown
  synth_save_default_config(synth);
#endif
  midi_shutdown(&synth->midi); 
}

void synth_set_bpm(Synth *synth, float bpm) {
  fx_set_bpm(&synth->fx, bpm);
}

void synth_audio_callback(void *userdata, Uint8 *stream, int len) {
  Synth *synth = (Synth *)userdata;
  const int frames = len / (sizeof(float) * 2);
  float *out = (float *)stream;
  memset(out, 0, sizeof(float) * frames * 2);
  
  synth_update_arpeggiator(synth, frames);
  synth_update_startup_melody(synth, frames); // Update the startup melody playback

  float *vbuf = (float *)calloc(frames * 2, sizeof(float));
  if (!vbuf)
    return; // or handle error
  for (int v = 0; v < synth->max_voices; ++v) {
    if (!synth->voices[v].active)
      continue;
    memset(vbuf, 0, sizeof(float) * frames * 2);
    voice_render(&synth->voices[v], synth->osc, synth->lfos, synth->mixer.osc_gain, vbuf, frames);
    for (int i = 0; i < frames * 2; ++i)
      out[i] += vbuf[i];
  }
  free(vbuf);

  mixer_apply(&synth->mixer, out, frames);
  ring_mod_process(&synth->ring_mod, out, frames);
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
    mixed_sample *= 4.0f; // Increased amplitude for oscilloscope display
	oscilloscope_feed(mixed_sample);
    oscilloscope_update_fft(mixed_sample);
    
    // Track max amplitude for debugging
    float abs_sample = fabsf(mixed_sample);
    if (abs_sample > max_sample) max_sample = abs_sample;
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
    if (i >= 0 && i < 4) {
      // Handle gain parameter specially - route to mixer instead of oscillator
      if (strncmp(param + 5, "gain", 4) == 0) {
        // Set mixer gain for this oscillator
        synth->mixer.osc_gain[i] = value;
      } else {
        // Set other oscillator parameters normally
        osc_set_param(&synth->osc[i], param + 5, value);
      }
    }
  } else if (strncmp(param, "fx.", 3) == 0) {
    fx_set_param(&synth->fx, param + 3, value);
  } else if (strncmp(param, "mixer.", 6) == 0) {
    mixer_set_param(&synth->mixer, param + 6, value);
  } else if (strncmp(param, "arp.", 4) == 0) {
    arpeggiator_set_param(&synth->arp, param + 4, value, synth);
  } else if (strncmp(param, "adsr.", 5) == 0) {
    const char *sub_param = param + 5;
    if (strcmp(sub_param, "attack") == 0) {
      synth->adsr.attack = value;
      // Update all voices
      for (int v = 0; v < synth->max_voices; ++v) {
        synth->voices[v].adsr.attack = value;
      }
    } else if (strcmp(sub_param, "decay") == 0) {
      synth->adsr.decay = value;
      for (int v = 0; v < synth->max_voices; ++v) {
        synth->voices[v].adsr.decay = value;
      }
    } else if (strcmp(sub_param, "sustain") == 0) {
      synth->adsr.sustain = value;
      for (int v = 0; v < synth->max_voices; ++v) {
        synth->voices[v].adsr.sustain = value;
      }
    } else if (strcmp(sub_param, "release") == 0) {
      synth->adsr.release = value;
      for (int v = 0; v < synth->max_voices; ++v) {
        synth->voices[v].adsr.release = value;
      }
    }
  } else if (strncmp(param, "lfo", 3) == 0) {
    int lfo_num = param[3] - '1';
    if (lfo_num >= 0 && lfo_num < 3) {
      lfo_set_param(&synth->lfos[lfo_num], param + 5, value);
    }
  } else if (strncmp(param, "ring_mod.", 9) == 0) {
    const char *sub_param = param + 9;
    if (strcmp(sub_param, "frequency") == 0) {
      ring_mod_set_frequency(&synth->ring_mod, value);
    } else if (strcmp(sub_param, "mix") == 0) {
      ring_mod_set_mix(&synth->ring_mod, value);
    } else if (strcmp(sub_param, "enabled") == 0) {
      ring_mod_set_enabled(&synth->ring_mod, (int)value);
    }
  }
}

void synth_note_on(Synth *synth, int note, float velocity) {
  synth->timestamp_counter++; // Increment timestamp for new note

  // Trigger LFO sync on note on
  for (int l = 0; l < 3; ++l) {
    lfo_note_on(&synth->lfos[l]);
  }

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
      
      // Trigger LFO sync on note off
      for (int l = 0; l < 3; ++l) {
        lfo_note_off(&synth->lfos[l]);
      }
      return;
    }
  }
}

#include "cJSON.h" // Include cJSON header

char* synth_save_preset_json(const Synth *synth) {
    cJSON *root = cJSON_CreateObject();

    // Save Oscillator parameters
    cJSON *oscillators = cJSON_CreateArray();
    for (int i = 0; i < 4; ++i) {
        cJSON *osc = cJSON_CreateObject();
        cJSON_AddNumberToObject(osc, "waveform", synth->osc[i].waveform);
        cJSON_AddNumberToObject(osc, "pitch", synth->osc[i].pitch);
        cJSON_AddNumberToObject(osc, "detune", synth->osc[i].detune);
        cJSON_AddNumberToObject(osc, "gain", synth->osc[i].gain);
        cJSON_AddNumberToObject(osc, "pan", synth->osc[i].pan);
        cJSON_AddNumberToObject(osc, "pulse_width", synth->osc[i].pulse_width);
        cJSON_AddNumberToObject(osc, "unison_voices", synth->osc[i].unison_voices);
        cJSON_AddNumberToObject(osc, "unison_detune", synth->osc[i].unison_detune);
        cJSON_AddItemToArray(oscillators, osc);
    }
cJSON_AddItemToObject(root, "oscillators", oscillators);
 
    // Save ADSR Envelope parameters
    cJSON *adsr = cJSON_CreateObject();
    cJSON_AddNumberToObject(adsr, "attack", synth->adsr.attack);
    cJSON_AddNumberToObject(adsr, "decay", synth->adsr.decay);
    cJSON_AddNumberToObject(adsr, "sustain", synth->adsr.sustain);
    cJSON_AddNumberToObject(adsr, "release", synth->adsr.release);
    cJSON_AddItemToObject(root, "adsr", adsr);

    // Save LFO parameters
    cJSON *lfos = cJSON_CreateArray();
    for (int i = 0; i < 3; ++i) {
        cJSON *lfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(lfo, "waveform", synth->lfos[i].waveform);
        cJSON_AddNumberToObject(lfo, "frequency", synth->lfos[i].frequency);
        cJSON_AddNumberToObject(lfo, "depth", synth->lfos[i].depth);
        cJSON_AddNumberToObject(lfo, "phase", synth->lfos[i].phase);
        cJSON_AddNumberToObject(lfo, "gain", synth->lfos[i].gain);
        cJSON_AddNumberToObject(lfo, "target", synth->lfos[i].target);
        cJSON_AddNumberToObject(lfo, "sync", synth->lfos[i].sync);
        cJSON_AddBoolToObject(lfo, "enabled", synth->lfos[i].enabled);
        cJSON_AddItemToArray(lfos, lfo);
    }
    cJSON_AddItemToObject(root, "lfos", lfos);

    // Save Mixer parameters
    cJSON *mixer = cJSON_CreateObject();
    cJSON_AddNumberToObject(mixer, "master_gain", synth->mixer.master);
    cJSON_AddNumberToObject(mixer, "master_pan", synth->mixer.master_pan);
    cJSON_AddNumberToObject(mixer, "master_width", synth->mixer.master_width);
    cJSON *osc_gains = cJSON_CreateArray();
    for (int i = 0; i < 4; ++i) {
        cJSON_AddItemToArray(osc_gains, cJSON_CreateNumber(synth->mixer.osc_gain[i]));
    }
    cJSON_AddItemToObject(mixer, "osc_gains", osc_gains);
    // Add compressor parameters
    cJSON_AddNumberToObject(mixer, "comp_enabled", synth->mixer.comp_enabled);
    cJSON_AddNumberToObject(mixer, "comp_threshold", synth->mixer.comp_threshold);
    cJSON_AddNumberToObject(mixer, "comp_ratio", synth->mixer.comp_ratio);
    cJSON_AddNumberToObject(mixer, "comp_attack", synth->mixer.comp_attack);
    cJSON_AddNumberToObject(mixer, "comp_release", synth->mixer.comp_release);
    cJSON_AddNumberToObject(mixer, "comp_makeup_gain", synth->mixer.comp_makeup_gain);
    cJSON_AddNumberToObject(mixer, "dc_filter_enabled", synth->mixer.dc_filter_enabled);
    cJSON_AddNumberToObject(mixer, "dc_filter_freq", synth->mixer.dc_filter_freq);
    cJSON_AddNumberToObject(mixer, "soft_clip_enabled", synth->mixer.soft_clip_enabled);
    cJSON_AddNumberToObject(mixer, "soft_clip_threshold", synth->mixer.soft_clip_threshold);
    cJSON_AddNumberToObject(mixer, "soft_clip_ratio", synth->mixer.soft_clip_ratio);
    cJSON_AddNumberToObject(mixer, "auto_gain_enabled", synth->mixer.auto_gain_enabled);
    cJSON_AddNumberToObject(mixer, "auto_gain_target", synth->mixer.auto_gain_target);
    cJSON_AddItemToObject(root, "mixer", mixer);

    // Save FX parameters
    cJSON *fx = cJSON_CreateObject();
    cJSON_AddNumberToObject(fx, "filter_enabled", synth->fx.filter_enabled);
    cJSON_AddNumberToObject(fx, "filter_cutoff", synth->fx.filter_cutoff);
    cJSON_AddNumberToObject(fx, "filter_resonance", synth->fx.filter_resonance);
    cJSON_AddNumberToObject(fx, "filter_drive", synth->fx.filter_drive);
    cJSON_AddNumberToObject(fx, "filter_mix", synth->fx.filter_mix);
    cJSON_AddNumberToObject(fx, "filter_oversampling", synth->fx.filter_oversampling);
    cJSON_AddNumberToObject(fx, "flanger_depth", synth->fx.flanger_depth);
    cJSON_AddNumberToObject(fx, "flanger_rate", synth->fx.flanger_rate);
    cJSON_AddNumberToObject(fx, "flanger_feedback", synth->fx.flanger_feedback);
    cJSON_AddNumberToObject(fx, "delay_time", synth->fx.delay_time);
    cJSON_AddNumberToObject(fx, "delay_feedback", synth->fx.delay_feedback);
    cJSON_AddNumberToObject(fx, "delay_mix", synth->fx.delay_mix);
    cJSON_AddNumberToObject(fx, "multitap_enabled", synth->fx.multitap_enabled);
    cJSON_AddNumberToObject(fx, "multitap_tap0", synth->fx.multitap_taps[0]);
    cJSON_AddNumberToObject(fx, "multitap_tap0_level", synth->fx.multitap_levels[0]);
    cJSON_AddNumberToObject(fx, "multitap_tap1", synth->fx.multitap_taps[1]);
    cJSON_AddNumberToObject(fx, "multitap_tap1_level", synth->fx.multitap_levels[1]);
    cJSON_AddNumberToObject(fx, "multitap_tap2", synth->fx.multitap_taps[2]);
    cJSON_AddNumberToObject(fx, "multitap_tap2_level", synth->fx.multitap_levels[2]);
    cJSON_AddNumberToObject(fx, "multitap_tap3", synth->fx.multitap_taps[3]);
    cJSON_AddNumberToObject(fx, "multitap_tap3_level", synth->fx.multitap_levels[3]);
    cJSON_AddNumberToObject(fx, "reverb_size", synth->fx.reverb_size);
    cJSON_AddNumberToObject(fx, "reverb_mix", synth->fx.reverb_damping);
    cJSON_AddNumberToObject(fx, "reverb_damping", synth->fx.reverb_mix);
    cJSON_AddItemToObject(root, "fx", fx);

    // Save Ring Modulator parameters
    cJSON *ring_mod = cJSON_CreateObject();
    cJSON_AddNumberToObject(ring_mod, "frequency", synth->ring_mod.frequency);
    cJSON_AddNumberToObject(ring_mod, "mix", synth->ring_mod.mix);
    cJSON_AddBoolToObject(ring_mod, "enabled", synth->ring_mod.enabled);
    cJSON_AddItemToObject(root, "ring_modulator", ring_mod);

    // Save Arpeggiator parameters
    cJSON *arp = cJSON_CreateObject();
    cJSON_AddBoolToObject(arp, "enabled", synth->arp.enabled);
    cJSON_AddNumberToObject(arp, "mode", synth->arp.mode);
    cJSON_AddNumberToObject(arp, "tempo", synth->arp.tempo);
    cJSON_AddNumberToObject(arp, "rate", synth->arp.rate);
    cJSON_AddBoolToObject(arp, "polyphonic", synth->arp.polyphonic);
    cJSON_AddBoolToObject(arp, "hold", synth->arp.hold);
    cJSON_AddNumberToObject(arp, "octave", synth->arp.octave);
    cJSON_AddNumberToObject(arp, "octaves", synth->arp.octaves);
    
    // Chord generation parameters
    cJSON_AddNumberToObject(arp, "chord_type", synth->arp.chord_type);
    cJSON_AddBoolToObject(arp, "add_6", synth->arp.add_6);
    cJSON_AddBoolToObject(arp, "add_m7", synth->arp.add_m7);
    cJSON_AddBoolToObject(arp, "add_M7", synth->arp.add_M7);
    cJSON_AddBoolToObject(arp, "add_9", synth->arp.add_9);
    cJSON_AddNumberToObject(arp, "voicing", synth->arp.voicing);
    
    // Gate parameters
    cJSON_AddNumberToObject(arp, "gate_length", synth->arp.gate_length);
    
    cJSON_AddItemToObject(root, "arpeggiator", arp);

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);
    return json_string;
}

void synth_load_preset_json(Synth *synth, const char *json_string) {
    cJSON *root = cJSON_Parse(json_string);
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return;
    }

    // Load Oscillator parameters
    cJSON *oscillators = cJSON_GetObjectItemCaseSensitive(root, "oscillators");
    if (cJSON_IsArray(oscillators)) {
        int num_oscillators = cJSON_GetArraySize(oscillators);
        for (int i = 0; i < num_oscillators && i < 4; ++i) { // Limit to 4 oscillators
            cJSON *osc = cJSON_GetArrayItem(oscillators, i);
            if (cJSON_IsObject(osc)) {
                cJSON *waveform = cJSON_GetObjectItemCaseSensitive(osc, "waveform");
                if (cJSON_IsNumber(waveform)) {
                    char param_name[32];
                    snprintf(param_name, sizeof(param_name), "osc%d.waveform", i + 1);
                    synth_set_param(synth, param_name, (float)waveform->valuedouble);
                }
                cJSON *pitch = cJSON_GetObjectItemCaseSensitive(osc, "pitch");
                if (cJSON_IsNumber(pitch)) {
                    char param_name[32];
                    snprintf(param_name, sizeof(param_name), "osc%d.pitch", i + 1);
                    synth_set_param(synth, param_name, (float)pitch->valuedouble);
                }
                cJSON *detune = cJSON_GetObjectItemCaseSensitive(osc, "detune");
                if (cJSON_IsNumber(detune)) {
                    char param_name[32];
                    snprintf(param_name, sizeof(param_name), "osc%d.detune", i + 1);
                    synth_set_param(synth, param_name, (float)detune->valuedouble);
                }
                cJSON *gain = cJSON_GetObjectItemCaseSensitive(osc, "gain");
                if (cJSON_IsNumber(gain)) {
                    char param_name[32];
                    snprintf(param_name, sizeof(param_name), "osc%d.gain", i + 1);
                    synth_set_param(synth, param_name, (float)gain->valuedouble);
                }
            }
        }
    }

    // Load Mixer parameters
    cJSON *mixer = cJSON_GetObjectItemCaseSensitive(root, "mixer");
    if (cJSON_IsObject(mixer)) {
        cJSON *master_gain = cJSON_GetObjectItemCaseSensitive(mixer, "master_gain");
        if (cJSON_IsNumber(master_gain)) {
            synth_set_param(synth, "mixer.master", (float)master_gain->valuedouble);
        }
        cJSON *osc_gains = cJSON_GetObjectItemCaseSensitive(mixer, "osc_gains");
        if (cJSON_IsArray(osc_gains)) {
            int num_osc_gains = cJSON_GetArraySize(osc_gains);
            for (int i = 0; i < num_osc_gains && i < 4; ++i) { // Limit to 4 osc gains
                cJSON *gain = cJSON_GetArrayItem(osc_gains, i);
                if (cJSON_IsNumber(gain)) {
                    char param_name[32];
                    snprintf(param_name, sizeof(param_name), "mixer.osc%d", i + 1);
                    synth_set_param(synth, param_name, (float)gain->valuedouble);
                }
            }
        }
        cJSON *comp_threshold = cJSON_GetObjectItemCaseSensitive(mixer, "comp_threshold");
        if (cJSON_IsNumber(comp_threshold)) {
            synth_set_param(synth, "mixer.comp.threshold", (float)comp_threshold->valuedouble);
        }
        cJSON *comp_ratio = cJSON_GetObjectItemCaseSensitive(mixer, "comp_ratio");
        if (cJSON_IsNumber(comp_ratio)) {
            synth_set_param(synth, "mixer.comp.ratio", (float)comp_ratio->valuedouble);
        }
        cJSON *comp_attack = cJSON_GetObjectItemCaseSensitive(mixer, "comp_attack");
        if (cJSON_IsNumber(comp_attack)) {
            synth_set_param(synth, "mixer.comp.attack", (float)comp_attack->valuedouble);
        }
        cJSON *comp_release = cJSON_GetObjectItemCaseSensitive(mixer, "comp_release");
        if (cJSON_IsNumber(comp_release)) {
            synth_set_param(synth, "mixer.comp.release", (float)comp_release->valuedouble);
        }
        cJSON *comp_makeup_gain = cJSON_GetObjectItemCaseSensitive(mixer, "comp_makeup_gain");
        if (cJSON_IsNumber(comp_makeup_gain)) {
            synth_set_param(synth, "mixer.comp.makeup", (float)comp_makeup_gain->valuedouble);
        }
    }

    // Load FX parameters
    cJSON *fx = cJSON_GetObjectItemCaseSensitive(root, "fx");
    if (cJSON_IsObject(fx)) {
        cJSON *flanger_depth = cJSON_GetObjectItemCaseSensitive(fx, "flanger_depth");
        if (cJSON_IsNumber(flanger_depth)) {
            synth_set_param(synth, "flanger.depth", (float)flanger_depth->valuedouble);
        }
        cJSON *flanger_rate = cJSON_GetObjectItemCaseSensitive(fx, "flanger_rate");
        if (cJSON_IsNumber(flanger_rate)) {
            synth_set_param(synth, "flanger.rate", (float)flanger_rate->valuedouble);
        }
        cJSON *flanger_feedback = cJSON_GetObjectItemCaseSensitive(fx, "flanger_feedback");
        if (cJSON_IsNumber(flanger_feedback)) {
            synth_set_param(synth, "flanger.feedback", (float)flanger_feedback->valuedouble);
        }
        cJSON *delay_time = cJSON_GetObjectItemCaseSensitive(fx, "delay_time");
        if (cJSON_IsNumber(delay_time)) {
            synth_set_param(synth, "delay.time", (float)delay_time->valuedouble);
        }
        cJSON *delay_feedback = cJSON_GetObjectItemCaseSensitive(fx, "delay_feedback");
        if (cJSON_IsNumber(delay_feedback)) {
            synth_set_param(synth, "delay.feedback", (float)delay_feedback->valuedouble);
        }
        cJSON *delay_mix = cJSON_GetObjectItemCaseSensitive(fx, "delay_mix");
        if (cJSON_IsNumber(delay_mix)) {
            synth_set_param(synth, "delay.mix", (float)delay_mix->valuedouble);
        }
        cJSON *reverb_size = cJSON_GetObjectItemCaseSensitive(fx, "reverb_size");
        if (cJSON_IsNumber(reverb_size)) {
            synth_set_param(synth, "reverb.size", (float)reverb_size->valuedouble);
        }
        cJSON *reverb_damping = cJSON_GetObjectItemCaseSensitive(fx, "reverb_damping");
        if (cJSON_IsNumber(reverb_damping)) {
            synth_set_param(synth, "reverb.damping", (float)reverb_damping->valuedouble);
        }
        cJSON *reverb_mix = cJSON_GetObjectItemCaseSensitive(fx, "reverb_mix");
        if (cJSON_IsNumber(reverb_mix)) {
            synth_set_param(synth, "reverb.mix", (float)reverb_mix->valuedouble);
        }
    }

    // Load Ring Modulator parameters
    cJSON *ring_mod = cJSON_GetObjectItemCaseSensitive(root, "ring_modulator");
    if (cJSON_IsObject(ring_mod)) {
        cJSON *frequency = cJSON_GetObjectItemCaseSensitive(ring_mod, "frequency");
        if (cJSON_IsNumber(frequency)) {
            synth_set_param(synth, "ring_mod.frequency", (float)frequency->valuedouble);
        }
        cJSON *mix = cJSON_GetObjectItemCaseSensitive(ring_mod, "mix");
        if (cJSON_IsNumber(mix)) {
            synth_set_param(synth, "ring_mod.mix", (float)mix->valuedouble);
        }
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(ring_mod, "enabled");
        if (cJSON_IsBool(enabled)) {
            synth_set_param(synth, "ring_mod.enabled", (float)cJSON_IsTrue(enabled));
        }
    }

    // Load Arpeggiator parameters
    cJSON *arp = cJSON_GetObjectItemCaseSensitive(root, "arpeggiator");
    if (cJSON_IsObject(arp)) {
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(arp, "enabled");
        if (cJSON_IsBool(enabled)) {
            synth_set_param(synth, "arp.enabled", (float)cJSON_IsTrue(enabled));
        }
        cJSON *mode = cJSON_GetObjectItemCaseSensitive(arp, "mode");
        if (cJSON_IsNumber(mode)) {
            synth_set_param(synth, "arp.mode", (float)mode->valuedouble);
        }
        cJSON *tempo = cJSON_GetObjectItemCaseSensitive(arp, "tempo");
        if (cJSON_IsNumber(tempo)) {
            synth_set_param(synth, "arp.tempo", (float)tempo->valuedouble);
        }
        cJSON *rate = cJSON_GetObjectItemCaseSensitive(arp, "rate");
        if (cJSON_IsNumber(rate)) {
            synth_set_param(synth, "arp.rate", (float)rate->valuedouble);
        }
        cJSON *polyphonic = cJSON_GetObjectItemCaseSensitive(arp, "polyphonic");
        if (cJSON_IsBool(polyphonic)) {
            synth_set_param(synth, "arp.polyphonic", (float)cJSON_IsTrue(polyphonic));
        }
        cJSON *hold = cJSON_GetObjectItemCaseSensitive(arp, "hold");
        if (cJSON_IsBool(hold)) {
            synth_set_param(synth, "arp.hold", (float)cJSON_IsTrue(hold));
        }
        cJSON *octave = cJSON_GetObjectItemCaseSensitive(arp, "octave");
        if (cJSON_IsNumber(octave)) {
            synth_set_param(synth, "arp.octave", (float)octave->valuedouble);
        }
        cJSON *octaves = cJSON_GetObjectItemCaseSensitive(arp, "octaves");
        if (cJSON_IsNumber(octaves)) {
            synth_set_param(synth, "arp.octaves", (float)octaves->valuedouble);
        }
        
        // Chord generation parameters
        cJSON *chord_type = cJSON_GetObjectItemCaseSensitive(arp, "chord_type");
        if (cJSON_IsNumber(chord_type)) {
            synth_set_param(synth, "arp.chord_type", (float)chord_type->valuedouble);
        }
        cJSON *add_6 = cJSON_GetObjectItemCaseSensitive(arp, "add_6");
        if (cJSON_IsBool(add_6)) {
            synth_set_param(synth, "arp.add_6", (float)cJSON_IsTrue(add_6));
        }
        cJSON *add_m7 = cJSON_GetObjectItemCaseSensitive(arp, "add_m7");
        if (cJSON_IsBool(add_m7)) {
            synth_set_param(synth, "arp.add_m7", (float)cJSON_IsTrue(add_m7));
        }
        cJSON *add_M7 = cJSON_GetObjectItemCaseSensitive(arp, "add_M7");
        if (cJSON_IsBool(add_M7)) {
            synth_set_param(synth, "arp.add_M7", (float)cJSON_IsTrue(add_M7));
        }
        cJSON *add_9 = cJSON_GetObjectItemCaseSensitive(arp, "add_9");
        if (cJSON_IsBool(add_9)) {
            synth_set_param(synth, "arp.add_9", (float)cJSON_IsTrue(add_9));
        }
        cJSON *voicing = cJSON_GetObjectItemCaseSensitive(arp, "voicing");
        if (cJSON_IsNumber(voicing)) {
            synth_set_param(synth, "arp.voicing", (float)voicing->valuedouble);
        }
        cJSON *gate_length = cJSON_GetObjectItemCaseSensitive(arp, "gate_length");
        if (cJSON_IsNumber(gate_length)) {
            synth_set_param(synth, "arp.gate_length", (float)gate_length->valuedouble);
        }
    }

    cJSON_Delete(root);
}

void synth_save_default_config(const Synth *synth) {
    char *json_string = synth_save_preset_json(synth);
    if (!json_string) {
        fprintf(stderr, "Failed to generate JSON string for config.\n");
        return;
    }

    const char *config_filename = "default_config.json"; // Default config file name
    FILE *fp = fopen(config_filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open config file '%s' for writing.\n", config_filename);
        free(json_string);
        return;
    }

    fprintf(fp, "%s", json_string);
    fclose(fp);
    free(json_string);
    printf("Synth parameters saved to '%s'.\n", config_filename);
}

void synth_load_default_config(Synth *synth) {
    const char *config_filename = "default_config.json";
    FILE *fp = fopen(config_filename, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open config file '%s' for reading.\n", config_filename);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *json_string = (char *)malloc(file_size + 1);
    if (!json_string) {
        fprintf(stderr, "Failed to allocate memory for config file content.\n");
        fclose(fp);
        return;
    }

    if (fread(json_string, 1, file_size, fp) != file_size) {
        fprintf(stderr, "Failed to read config file content.\n");
        free(json_string);
        fclose(fp);
        return;
    }
    json_string[file_size] = '\0'; // Null-terminate the string
    fclose(fp);

    synth_load_preset_json(synth, json_string);
    free(json_string);
    printf("Synth parameters loaded from '%s'.\n", config_filename);
}

void synth_randomize_parameters(Synth *synth) {
    char param_name[32];
    
    // Randomize all oscillators (0-3)
    for (int i = 0; i < 4; i++) {
        // Randomize waveform (0-4: SINE, SAW, SQUARE, TRI, NOISE)
        float random_waveform = (float)(rand() % 5);
        snprintf(param_name, sizeof(param_name), "osc%d.waveform", i + 1);
        synth_set_param(synth, param_name, random_waveform);
        
        // Randomize pitch (-24 to +24 semitones)
        float random_pitch = -24.0f + ((float)rand() / (float)RAND_MAX) * 48.0f;
        snprintf(param_name, sizeof(param_name), "osc%d.pitch", i + 1);
        synth_set_param(synth, param_name, random_pitch);
        
        // Randomize detune (-1.0 to +1.0 semitones)
        float random_detune = -1.0f + ((float)rand() / (float)RAND_MAX) * 2.0f;
        snprintf(param_name, sizeof(param_name), "osc%d.detune", i + 1);
        synth_set_param(synth, param_name, random_detune);
        
        // Randomize gain (0.0 to 1.0)
        float random_gain = (float)rand() / (float)RAND_MAX;
        snprintf(param_name, sizeof(param_name), "osc%d.gain", i + 1);
        synth_set_param(synth, param_name, random_gain);
        
        // Randomize pulse width (0.0 to 1.0)
        float random_pulse_width = (float)rand() / (float)RAND_MAX;
        snprintf(param_name, sizeof(param_name), "osc%d.pulse_width", i + 1);
        synth_set_param(synth, param_name, random_pulse_width);
        
        // Randomize unison voices (1-8)
        int random_unison = 1 + (rand() % 8);
        snprintf(param_name, sizeof(param_name), "osc%d.unison_voices", i + 1);
        synth_set_param(synth, param_name, (float)random_unison);
        
        // Randomize unison detune (0.0 to 1.0)
        float random_unison_detune = (float)rand() / (float)RAND_MAX;
        snprintf(param_name, sizeof(param_name), "osc%d.unison_detune", i + 1);
        synth_set_param(synth, param_name, random_unison_detune);
    }
}

void synth_randomize_oscillator(Synth *synth, int osc_index) {
    if (osc_index < 0 || osc_index >= 4) return;
    
    char param_name[32];
    
    // Randomize waveform (0-4: SINE, SAW, SQUARE, TRI, NOISE)
    float random_waveform = (float)(rand() % 5);
    snprintf(param_name, sizeof(param_name), "osc%d.waveform", osc_index + 1);
    synth_set_param(synth, param_name, random_waveform);
    
    // Randomize pitch (-24 to +24 semitones)
    float random_pitch = -24.0f + ((float)rand() / (float)RAND_MAX) * 48.0f;
    snprintf(param_name, sizeof(param_name), "osc%d.pitch", osc_index + 1);
    synth_set_param(synth, param_name, random_pitch);
    
    // Randomize detune (-1.0 to +1.0 semitones)
    float random_detune = -1.0f + ((float)rand() / (float)RAND_MAX) * 2.0f;
    snprintf(param_name, sizeof(param_name), "osc%d.detune", osc_index + 1);
    synth_set_param(synth, param_name, random_detune);
    
    // Randomize gain (0.0 to 1.0)
    float random_gain = (float)rand() / (float)RAND_MAX;
    snprintf(param_name, sizeof(param_name), "osc%d.gain", osc_index + 1);
    synth_set_param(synth, param_name, random_gain);
    
    // Randomize pulse width (0.0 to 1.0)
    float random_pulse_width = (float)rand() / (float)RAND_MAX;
    snprintf(param_name, sizeof(param_name), "osc%d.pulse_width", osc_index + 1);
    synth_set_param(synth, param_name, random_pulse_width);
    
    // Randomize unison voices (1-8)
    int random_unison = 1 + (rand() % 8);
    snprintf(param_name, sizeof(param_name), "osc%d.unison_voices", osc_index + 1);
    synth_set_param(synth, param_name, (float)random_unison);
    
    // Randomize unison detune (0.0 to 1.0)
    float random_unison_detune = (float)rand() / (float)RAND_MAX;
    snprintf(param_name, sizeof(param_name), "osc%d.unison_detune", osc_index + 1);
    synth_set_param(synth, param_name, random_unison_detune);
}
