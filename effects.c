#include "effects.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

void effects_init(Effects *fx, int samplerate, int maxblock) {
    fx->samplerate = samplerate;
    fx->flanger.size = samplerate * 2;
    fx->flanger.buf = (float*)calloc(fx->flanger.size*2, sizeof(float));
    fx->flanger.pos = 0;
    fx->flanger.lfo_phase = 0.0f;

    fx->delay.size = samplerate * 2;
    fx->delay.buf = (float*)calloc(fx->delay.size*2, sizeof(float));
    fx->delay.pos = 0;

    fx->reverb.size = samplerate * 4;
    fx->reverb.buf = (float*)calloc(fx->reverb.size*2, sizeof(float));
    fx->reverb.pos = 0;
}

void effects_free(Effects *fx) {
    free(fx->flanger.buf);
    free(fx->delay.buf);
    free(fx->reverb.buf);
}

void effects_process(Effects *fx, float *buf, int ns, float fl_depth, float fl_rate, float delay_ms, float delay_fb, float reverb_mix) {
    // Flanger
    FlangerState *fl = &fx->flanger;
    int srate = fx->samplerate;
    for (int i=0; i<ns; ++i) {
        for (int c=0; c<2; ++c) {
            float dry = buf[i*2+c];
            float lfo = sinf(fl->lfo_phase * 2.0f * (float)M_PI);
            int fl_offset = (int)(fl_depth * 0.005f * srate * (1.0f + lfo));
            int fl_idx = (fl->pos - fl_offset + fl->size) % fl->size;
            float wet = fl->buf[fl_idx*2+c];
            fl->buf[fl->pos*2+c] = dry;
            buf[i*2+c] = dry + wet * 0.5f;
        }
        fl->pos = (fl->pos+1)%fl->size;
        fl->lfo_phase += fl_rate/(float)srate;
        if (fl->lfo_phase > 1.0f) fl->lfo_phase -= 1.0f;
    }
    // Delay
    DelayState *dl = &fx->delay;
    int delay_samp = (int)(delay_ms * srate * 0.001f);
    for (int i=0; i<ns; ++i) {
        for (int c=0; c<2; ++c) {
            float dry = buf[i*2+c];
            int dl_idx = (dl->pos - delay_samp + dl->size) % dl->size;
            float wet = dl->buf[dl_idx*2+c];
            dl->buf[dl->pos*2+c] = dry + wet * delay_fb;
            buf[i*2+c] = dry + wet * 0.6f;
        }
        dl->pos = (dl->pos+1)%dl->size;
    }
    // Reverb (Schroeder-style: 4 combs + 2 allpasses)
    // ReverbState *rv = &fx->reverb;
    static const int comb_delays[4] = {1116, 1188, 1277, 1356};
    static float comb_buf[4][4096*2] = {{0}};
    static int comb_pos[4] = {0};
    static float allpass_buf[2][700*2] = {{0}};
    static int allpass_pos[2] = {0};
    static const int allpass_delays[2] = {225, 556};
    for (int i=0; i<ns; ++i) {
        for (int c=0; c<2; ++c) {
            float inp = buf[i*2+c];
            float comb_sum = 0.0f;
            for (int j=0; j<4; ++j) {
                int idx = (comb_pos[j] - comb_delays[j] + 4096) % 4096;
                comb_sum += comb_buf[j][idx*2+c];
                comb_buf[j][comb_pos[j]*2+c] = inp + 0.773f * comb_buf[j][idx*2+c];
                comb_pos[j] = (comb_pos[j]+1)%4096;
            }
            float ap = comb_sum * 0.25f;
            for (int j=0; j<2; ++j) {
                int idx = (allpass_pos[j] - allpass_delays[j] + 700) % 700;
                float allp = allpass_buf[j][idx*2+c];
                float in = ap + 0.5f * allp;
                allpass_buf[j][allpass_pos[j]*2+c] = in;
                ap = allp - 0.5f * in;
                allpass_pos[j] = (allpass_pos[j]+1)%700;
            }
            float wet = ap;
            buf[i*2+c] = buf[i*2+c]*(1.0f-reverb_mix) + wet*reverb_mix;
        }
    }
}
