#include "lfo.h"
#include "utils.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

void lfo_init(LFO *lfo, float samplerate) {
  lfo->waveform = LFO_SINE;
  lfo->frequency = 1.0f;
  lfo->depth = 0.5f;
  lfo->phase = 0.0f;
  lfo->gain = 1.0f;
  lfo->samplerate = samplerate;
  lfo->phase_acc = 0.0f;
  lfo->target = LFO_TARGET_FREQUENCY;
  lfo->sync = LFO_SYNC_FREE;
  lfo->enabled = 0;
  lfo->notes_pressed = 0;
}

void lfo_set_param(LFO *lfo, const char *param, float value) {
  if (strcmp(param, "waveform") == 0)
    lfo->waveform = (LfoWaveform)((int)value);
  else if (strcmp(param, "frequency") == 0)
    lfo->frequency = value;
  else if (strcmp(param, "depth") == 0)
    lfo->depth = value;
  else if (strcmp(param, "phase") == 0)
    lfo->phase = value;
  else if (strcmp(param, "gain") == 0)
    lfo->gain = value;
  else if (strcmp(param, "target") == 0)
    lfo->target = (LfoTarget)((int)value);
  else if (strcmp(param, "sync") == 0)
    lfo->sync = (LfoSyncMode)((int)value);
  else if (strcmp(param, "enabled") == 0)
    lfo->enabled = (int)(value + 0.5f);
}

float lfo_process(LFO *lfo) {
  if (!lfo->enabled || lfo->frequency <= 0.0f) {
    return 0.0f;
  }
  
  // Update phase accumulator
  lfo->phase_acc += lfo->frequency / lfo->samplerate;
  if (lfo->phase_acc >= 1.0f)
    lfo->phase_acc -= 1.0f;
    
  float p = lfo->phase_acc + lfo->phase;
  if (p >= 1.0f)
    p -= 1.0f;
    
  float output = 0.0f;
  switch (lfo->waveform) {
  case LFO_SINE:
    output = fastsin(2.0f * 3.1415926535f * p);
    break;
  case LFO_TRIANGLE:
    output = 4.0f * fabsf(p - 0.5f) - 1.0f;
    break;
  case LFO_SQUARE:
    output = p < 0.5f ? 1.0f : -1.0f;
    break;
  case LFO_SAW:
    output = 2.0f * p - 1.0f;
    break;
  case LFO_RANDOM:
    output = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
    break;
  default:
    output = 0.0f;
  }
  
  return lfo->gain * output;
}

float lfo_get_modulation_value(LFO *lfo) {
  return lfo_process(lfo) * lfo->depth;
}

void lfo_note_on(LFO *lfo) {
  if (lfo->sync == LFO_SYNC_RETRIGGER) {
    lfo->phase_acc = 0.0f; // Reset phase on each note
  }
  lfo->notes_pressed++;
}

void lfo_note_off(LFO *lfo) {
  if (lfo->notes_pressed > 0) {
    lfo->notes_pressed--;
  }
}