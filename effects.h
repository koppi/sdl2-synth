#ifndef EFFECTS_H
#define EFFECTS_H

typedef struct {
    float *flanger_buf;
    int flanger_len, flanger_pos;
    float *delay_buf;
    int delay_len, delay_pos;
    float reverb_lastL, reverb_lastR;
} Effects;

void effects_init(Effects *fx, int samplerate, int blocksize);
void effects_free(Effects *fx);
void effects_process(
    Effects *fx, float *buf, int ns,
    float flanger_depth, float flanger_rate,
    float delay_ms, float delay_fb,
    float reverb_mix);

#endif