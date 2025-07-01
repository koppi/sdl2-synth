#include "effects.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void effects_init(Effects *fx, int samplerate, int blocksize) {
    fx->flanger_len = samplerate/10;
    fx->flanger_buf = calloc(fx->flanger_len*2, sizeof(float));
    fx->flanger_pos = 0;
    fx->delay_len = samplerate*2;
    fx->delay_buf = calloc(fx->delay_len*2, sizeof(float));
    fx->delay_pos = 0;
    fx->reverb_lastL = 0.f;
    fx->reverb_lastR = 0.f;
}
void effects_free(Effects *fx) {
    free(fx->flanger_buf); free(fx->delay_buf);
}

void effects_process(Effects *fx, float *buf, int ns,
    float flanger_depth, float flanger_rate,
    float delay_ms, float delay_fb,
    float reverb_mix)
{
    int ch = 2;
    // Flanger
    for(int i=0;i<ns;i++) {
        float t = (float)fx->flanger_pos / fx->flanger_len;
        float d = (1.0f + sinf(2.0f*M_PI*flanger_rate*t)) * (flanger_depth*fx->flanger_len/4.0f);
        int idx = (fx->flanger_pos - (int)d + fx->flanger_len) % fx->flanger_len;
        for(int c=0;c<ch;c++) {
            float dry = buf[i*ch+c];
            float wet = fx->flanger_buf[idx*ch+c];
            buf[i*ch+c] = dry*0.7f + wet*0.3f;
            fx->flanger_buf[fx->flanger_pos*ch+c] = dry;
        }
        fx->flanger_pos = (fx->flanger_pos+1)%fx->flanger_len;
    }
    // Delay
    int delay_samples = (int)(delay_ms * 0.001f * fx->delay_len / 2.0f);
    for(int i=0;i<ns;i++) {
        for(int c=0;c<ch;c++) {
            float dry = buf[i*ch+c];
            int di = (fx->delay_pos - delay_samples + fx->delay_len) % fx->delay_len;
            float wet = fx->delay_buf[di*ch+c];
            buf[i*ch+c] = dry*0.75f + wet*0.25f;
            fx->delay_buf[fx->delay_pos*ch+c] = dry + wet*delay_fb;
        }
        fx->delay_pos = (fx->delay_pos+1)%fx->delay_len;
    }
    // Reverb (very simple)
    for(int i=0;i<ns;i++) {
        float inL = buf[i*ch+0], inR = buf[i*ch+1];
        buf[i*ch+0] = inL*(1-reverb_mix) + fx->reverb_lastL*reverb_mix;
        buf[i*ch+1] = inR*(1-reverb_mix) + fx->reverb_lastR*reverb_mix;
        fx->reverb_lastL = inL;
        fx->reverb_lastR = inR;
    }
}