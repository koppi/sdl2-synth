#include "osc.h"
#include "utils.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

void osc_init(Oscillator *osc, float samplerate) {
  osc->waveform = OSC_SINE;
  osc->pitch = 0.0f;
  osc->detune = 0.0f;
  osc->gain = 0.25f;
  osc->samplerate = samplerate;
  osc->phase_acc = 0.0f;
  osc->pan = 0.0f; // Center pan by default
  osc->pulse_width = 0.5f;
  osc->unison_detune = 0.1f;
  osc->unison_voices = 1;
}

void osc_set_param(Oscillator *osc, const char *param, float value) {
  if (strcmp(param, "waveform") == 0)
    osc->waveform = (OscWaveform)((int)value);
  else if (strcmp(param, "pitch") == 0)
    osc->pitch = value;
  else if (strcmp(param, "phase") == 0)
    osc->phase = value;
  else if (strcmp(param, "detune") == 0)
    osc->detune = value;
  else if (strcmp(param, "gain") == 0)
    osc->gain = value;
  else if (strcmp(param, "pulse_width") == 0)
    osc->pulse_width = value;
  else if (strcmp(param, "unison_detune") == 0)
    osc->unison_detune = value;
  else if (strcmp(param, "unison_voices") == 0)
    osc->unison_voices = (int)(value + 0.5f); // Round to nearest int
  else if (strcmp(param, "pan") == 0)
    osc->pan = value;
}

float osc_process(Oscillator *osc, float note, float *phase_acc) {
  float output = 0.0f;
  
  // Process unison voices
  for (int voice = 0; voice < osc->unison_voices; ++voice) {
    // Calculate frequency with unison detuning
    float voice_detune = 0.0f;
    if (osc->unison_voices > 1 && osc->unison_detune > 0.0f) {
      // Spread voices symmetrically around center frequency
      float spread = (float)(voice - (osc->unison_voices - 1) * 0.5f) / (osc->unison_voices - 1);
      voice_detune = spread * osc->unison_detune;
    }
    
    float freq = 440.0f * powf(2.0f, (note + osc->pitch + osc->detune + voice_detune - 69.0f) / 12.0f);
    *phase_acc += freq / osc->samplerate;
    if (*phase_acc >= 1.0f)
      *phase_acc -= 1.0f;
      
    float p = *phase_acc + osc->phase;
    if (p >= 1.0f)
      p -= 1.0f;
      
    float voice_output = 0.0f;
    switch (osc->waveform) {
    case OSC_SINE:
      voice_output = fastsin(2.0f * 3.1415926535f * p);
      break;
    case OSC_SAW:
      voice_output = 2.0f * p - 1.0f;
      break;
    case OSC_SQUARE:
      // Use pulse_width for variable pulse width
      voice_output = p < osc->pulse_width ? 1.0f : -1.0f;
      break;
    case OSC_TRI:
      voice_output = 4.0f * fabsf(p - 0.5f) - 1.0f;
      break;
    case OSC_NOISE:
      voice_output = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f; // White noise
      break;
    default:
      voice_output = 0.0f;
    }
    
    output += voice_output;
  }
  
  // Average the unison voices and apply gain
  if (osc->unison_voices > 1) {
    output /= osc->unison_voices;
  }
  
  return osc->gain * output;
}