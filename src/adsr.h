#pragma once

typedef enum {
  ADSR_IDLE,
  ADSR_ATTACK,
  ADSR_DECAY,
  ADSR_SUSTAIN,
  ADSR_RELEASE
} AdsrPhase;

typedef struct {
  float attack;   // Attack time in seconds
  float decay;    // Decay time in seconds
  float sustain;  // Sustain level (0.0 to 1.0)
  float release;  // Release time in seconds
  
  // Runtime state
  AdsrPhase phase;
  float level;
  float time_in_phase;
  float sample_rate;
  int gate;       // Note on/off state
} AdsrEnvelope;

void adsr_init(AdsrEnvelope *env, float sample_rate);
void adsr_set_params(AdsrEnvelope *env, float attack, float decay, float sustain, float release);
void adsr_gate_on(AdsrEnvelope *env);
void adsr_gate_off(AdsrEnvelope *env);
float adsr_process(AdsrEnvelope *env, int frames);
void adsr_reset(AdsrEnvelope *env);