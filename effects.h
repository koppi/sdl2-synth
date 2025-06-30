#ifndef EFFECTS_H
#define EFFECTS_H

// --- Flanger ---
typedef struct {
    float *buf;
    int size;
    int pos;
    float lfo_phase;
} FlangerState;

typedef struct {
    float *buf;
    int size;
    int pos;
} DelayState;

typedef struct {
    float *buf;
    int size;
    int pos;
} ReverbState;

// Full Effects struct definition
typedef struct Effects_ {
    FlangerState flanger;
    DelayState delay;
    ReverbState reverb;
    int samplerate;
} Effects;

// Initialize effects engine
void effects_init(Effects *fx, int samplerate, int maxblock);

// Free effect buffers
void effects_free(Effects *fx);

// Process effects (in-place stereo buffer)
// fl_depth: flanger depth [0.0..1.0]
// fl_rate: flanger rate [Hz]
// delay_ms: delay time [ms]
// delay_fb: delay feedback [0.0..1.0]
// reverb_mix: dry/wet mix [0.0..1.0]
void effects_process(Effects *fx, float *buf, int ns,
    float fl_depth, float fl_rate,
    float delay_ms, float delay_fb,
    float reverb_mix);

#endif