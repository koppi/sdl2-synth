#include "synth.h"
#include <string.h>
#include <time.h>

int synth_init(Synth *synth, int samplerate, int buffer_size, int voices) {
  memset(synth, 0, sizeof(Synth));
  synth->max_voices = voices;
  mixer_init(&synth->mixer);
  fx_init(&synth->fx, samplerate);
  arpeggiator_init(&synth->arp);
  midi_init(&synth->midi, synth);
  for (int i = 0; i < 4; ++i)
    osc_init(&synth->osc[i], samplerate);
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

  for (int n = 0; n < frames; ++n) {
	  //XXX oscilloscope_feed(0.5f * (out[n * 2 + 0] + out[n * 2 + 1]));
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
