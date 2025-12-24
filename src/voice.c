#include "voice.h"
#include <string.h>

void voice_init(Voice *v, float samplerate) {
  v->active = 0;
  v->note = 0;
  v->velocity = 0;
  v->timestamp = 0;
  memset(v->phase_acc, 0, sizeof(v->phase_acc));
  adsr_init(&v->adsr, samplerate);
}

void voice_on(Voice *v, float note, float velocity,
              unsigned long long timestamp) {
  v->active = 1;
  v->note = note;
  v->velocity = velocity;
  v->timestamp = timestamp;
  memset(v->phase_acc, 0, sizeof(v->phase_acc));
  adsr_gate_on(&v->adsr);
}

void voice_off(Voice *v) { 
  adsr_gate_off(&v->adsr);
  // Voice stays active until ADSR reaches IDLE state
}

void voice_render(Voice *v, const Oscillator *osc, const LFO *lfo, const float *osc_gains, float *stereo, int frames) {
  // Get ADSR envelope value for this block
  float adsr_value = adsr_process(&v->adsr, frames);
  
  // Check if voice has finished release phase
  if (v->adsr.phase == ADSR_IDLE) {
    v->active = 0;
    return;
  }
  
  for (int n = 0; n < frames; ++n) {
    float left = 0.0f;
    float right = 0.0f;
    
    // Get current LFO modulation values
    float pitch_mod = lfo[0].enabled ? lfo_get_modulation_value((LFO*)&lfo[0]) : 0.0f;
    float volume_mod = lfo[1].enabled ? lfo_get_modulation_value((LFO*)&lfo[1]) : 0.0f;
    float filter_mod = lfo[2].enabled ? lfo_get_modulation_value((LFO*)&lfo[2]) : 0.0f;
    
    // Process each oscillator with gain and panning
    for (int o = 0; o < 4; ++o) {
      float modified_note = v->note;
      
      // Apply pitch LFO (only to main oscillators 0-3)
      if (o < 4 && lfo[0].enabled) {
        modified_note += pitch_mod * 12.0f; // +/- 1 octave modulation range
      }
      
      float osc_output = osc_process((Oscillator *)&osc[o], modified_note, &v->phase_acc[o]);
      
      // Apply volume LFO
      if (lfo[1].enabled) {
        osc_output *= (1.0f + volume_mod * 0.5f); // +/- 50% volume modulation
      }
      
      osc_output *= osc_gains[o];
      
      // Apply oscillator panning
      float pan = (o < 4) ? osc[o].pan : 0.0f; // All oscillators support pan
      float left_gain = (pan <= 0.0f) ? 1.0f : 1.0f - pan;
      float right_gain = (pan >= 0.0f) ? 1.0f : 1.0f + pan;
      
      left += osc_output * left_gain;
      right += osc_output * right_gain;
    }
    
    // Apply velocity and ADSR envelope
    left *= v->velocity * adsr_value;
    right *= v->velocity * adsr_value;
    
    // Add to stereo buffer (no averaging - oscillator gains handle mixing)
    stereo[n * 2 + 0] += left; // Left channel
    stereo[n * 2 + 1] += right; // Right channel
  }
}

int voice_is_active(const Voice *v) {
  return v->active;
}
