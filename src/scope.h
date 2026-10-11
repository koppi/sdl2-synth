#pragma once

// Lock-free-ish waveform tap used to feed the oscilloscope display.
//
// The audio thread appends samples with scope_feed(); the UI thread reads the
// ring buffer when drawing. Races are benign (worst case: one stale sample on
// screen), so no synchronisation is used.

#define SCOPE_SIZE 2048

typedef struct ScopeBuffer {
  float samples[SCOPE_SIZE];
  int write_pos;
  int ready;
} ScopeBuffer;

static inline void scope_init(ScopeBuffer *s) {
  for (int i = 0; i < SCOPE_SIZE; ++i)
    s->samples[i] = 0.0f;
  s->write_pos = 0;
  s->ready = 0;
}

static inline void scope_feed(ScopeBuffer *s, float sample) {
  if (sample > 1.0f) sample = 1.0f;
  if (sample < -1.0f) sample = -1.0f;
  s->samples[s->write_pos] = sample;
  s->write_pos = (s->write_pos + 1) % SCOPE_SIZE;
  s->ready = 1;
}
