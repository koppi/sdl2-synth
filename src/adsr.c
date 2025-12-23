#include "adsr.h"

void adsr_init(AdsrEnvelope *env, float sample_rate) {
  env->sample_rate = sample_rate;
  env->phase = ADSR_IDLE;
  env->level = 0.0f;
  env->time_in_phase = 0.0f;
  env->gate = 0;
  
  // Default ADSR parameters
  env->attack = 0.1f;   // 100ms
  env->decay = 0.2f;    // 200ms
  env->sustain = 0.7f;  // 70%
  env->release = 0.3f;  // 300ms
}

void adsr_set_params(AdsrEnvelope *env, float attack, float decay, float sustain, float release) {
  env->attack = attack;
  env->decay = decay;
  env->sustain = sustain;
  env->release = release;
}

void adsr_gate_on(AdsrEnvelope *env) {
  env->gate = 1;
  env->phase = ADSR_ATTACK;
  env->time_in_phase = 0.0f;
  
  // If we were in release, continue from current level
  if (env->level == 0.0f) {
    env->level = 0.0f;
  }
}

void adsr_gate_off(AdsrEnvelope *env) {
  env->gate = 0;
  if (env->phase != ADSR_IDLE) {
    env->phase = ADSR_RELEASE;
    env->time_in_phase = 0.0f;
  }
}

float adsr_process(AdsrEnvelope *env, int frames) {
  float target_level = 0.0f;
  float time_increment = 1.0f / env->sample_rate;
  
  switch (env->phase) {
    case ADSR_IDLE:
      env->level = 0.0f;
      break;
      
    case ADSR_ATTACK:
      target_level = 1.0f;
      env->time_in_phase += time_increment * frames;
      
      if (env->attack <= 0.001f) {
        env->level = 1.0f;
        env->phase = ADSR_DECAY;
        env->time_in_phase = 0.0f;
      } else if (env->time_in_phase >= env->attack) {
        env->level = 1.0f;
        env->phase = ADSR_DECAY;
        env->time_in_phase = 0.0f;
      } else {
        env->level = env->time_in_phase / env->attack;
      }
      break;
      
    case ADSR_DECAY:
      target_level = env->sustain;
      env->time_in_phase += time_increment * frames;
      
      if (env->decay <= 0.001f) {
        env->level = env->sustain;
        env->phase = ADSR_SUSTAIN;
        env->time_in_phase = 0.0f;
      } else if (env->time_in_phase >= env->decay) {
        env->level = env->sustain;
        env->phase = ADSR_SUSTAIN;
        env->time_in_phase = 0.0f;
      } else {
        float decay_progress = env->time_in_phase / env->decay;
        env->level = 1.0f + (env->sustain - 1.0f) * decay_progress;
      }
      break;
      
    case ADSR_SUSTAIN:
      env->level = env->sustain;
      if (!env->gate) {
        env->phase = ADSR_RELEASE;
        env->time_in_phase = 0.0f;
      }
      break;
      
    case ADSR_RELEASE:
      target_level = 0.0f;
      env->time_in_phase += time_increment * frames;
      
      if (env->release <= 0.001f) {
        env->level = 0.0f;
        env->phase = ADSR_IDLE;
        env->time_in_phase = 0.0f;
      } else if (env->time_in_phase >= env->release) {
        env->level = 0.0f;
        env->phase = ADSR_IDLE;
        env->time_in_phase = 0.0f;
      } else {
        float release_progress = env->time_in_phase / env->release;
        env->level = env->sustain * (1.0f - release_progress);
      }
      break;
  }
  
  return env->level;
}

void adsr_reset(AdsrEnvelope *env) {
  env->phase = ADSR_IDLE;
  env->level = 0.0f;
  env->time_in_phase = 0.0f;
  env->gate = 0;
}