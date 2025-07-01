#include "synth.h"
#include "voice.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

const char* waveform_names[NUM_WAVEFORMS] = {
    "Saw", "Square", "Triangle", "Sine", "Pulse", "Noise"
};

// --- Polyphonic demo pattern ---
static const struct {
    double time;    // seconds since pattern start
    int notes[4];   // up to 4 simultaneous notes, -1 for unused
    double length;  // duration this step lasts
} demo_pattern[] = {
    {0.0,  {60, 64, 67, -1}, 0.5},  // Cmaj
    {0.5,  {62, 65, 69, -1}, 0.5},  // Dm
    {1.0,  {59, 62, 67, -1}, 0.5},  // G7/B
    {1.5,  {60, 64, 67, -1}, 0.5},  // Cmaj
    {2.0,  {65, 69, 72, -1}, 0.5},  // F
    {2.5,  {64, 67, 71, -1}, 0.5},  // Em
    {3.0,  {62, 65, 69, -1}, 0.5},  // Dm
    {3.5,  {60, 64, 67, -1}, 0.5},  // Cmaj
};
#define DEMO_PATTERN_LEN (sizeof(demo_pattern)/sizeof(demo_pattern[0]))
#define DEMO_LOOP_TIME 4.0

#define CHANNELS 2
#define POLYPHONY 32
#define SAMPLE_RATE 48000
#define BUFFER_SIZE 512

void synth_demo_update(Synth *s, double dt) {
    if (!s->demo_playing) return;
    s->demo_timer += dt;
    double t = fmod(s->demo_timer, DEMO_LOOP_TIME);

    // Find which pattern step we are in
    int step = 0;
    double acc = 0.0;
    for (int i = 0; i < DEMO_PATTERN_LEN; ++i) {
        acc += demo_pattern[i].length;
        if (t < acc) { step = i; break; }
    }
    if (step != s->demo_step) {
        // Release previous notes
        for (int i = 0; i < 8; ++i) {
            if (s->demo_notes_on[i] >= 0) {
                synth_note_off(s, s->demo_notes_on[i]);
                s->demo_notes_on[i] = -1;
            }
        }
        // Play new notes
        for (int i = 0; i < 4; ++i) {
            int n = demo_pattern[step].notes[i];
            if (n >= 0) {
                synth_note_on(s, n);
                s->demo_notes_on[i] = n;
            }
        }
        s->demo_step = step;
    }
}

static void audio_callback(void *udata, Uint8 *stream, int len) {
    Synth *s = (Synth*)udata;
    float *out = (float*)stream;
    int samples = len / sizeof(float) / 2;
    memset(out, 0, sizeof(float)*samples*2);
    Voice* v = s->voices;
    for (int vi=0; vi<s->voice_count; ++vi) {
        if (!v[vi].on) continue;
        float freq = 440.0f * powf(2.0f, (v[vi].note-69)/12.0f) * powf(2.0f,v[vi].detune/12.0f);
        for (int i=0;i<samples;++i) {
            float t = v[vi].phase + v[vi].osc_phase;
            float sample = 0;
            switch(v[vi].waveform) {
                case WAVE_SAW:
                    sample = 2.0f*(t - floorf(t+0.5f));
                    break;
                case WAVE_SQUARE:
                    sample = (fmodf(t,1.0f)<0.5f) ? 1.0f : -1.0f;
                    break;
                case WAVE_TRIANGLE:
                    sample = 2.0f*fabsf(2.0f*(t-floorf(t+0.5f)))-1.0f;
                    break;
                case WAVE_SINE:
                    sample = sinf(2*M_PI*t);
                    break;
                case WAVE_PULSE:
                    sample = (fmodf(t,1.0f)<0.2f) ? 1.0f : -1.0f; // 20% duty
                    break;
                case WAVE_NOISE:
                    sample = ((rand()%2000)-1000)/1000.0f; // crude
                    break;
                default:
                    sample = 0;
                    break;
            }
            out[2*i+0] += 0.2f * sample * v[vi].velocity;
            out[2*i+1] += 0.2f * sample * v[vi].velocity;
            v[vi].phase += freq/(float)SAMPLE_RATE;
            if (v[vi].phase > 1.0f) v[vi].phase -= 1.0f;
        }
    }
}

void synth_init(Synth *s) {
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < NUM_OSCS; ++i) {
        s->osc_wave[i] = 0; // Saw
        s->osc_detune[i] = (i == 0) ? 0.0f : (i == 1) ? 0.10f : (i == 2) ? -0.12f : 0.15f;
        s->osc_phase[i] = 0.0f;
    }
    s->volume = 100;
    s->octave = 4;
    s->demo_playing = 0;
    s->flanger_depth = 0.5f;
    s->delay_ms = 350.0f;
    s->reverb_mix = 0.18f;
    s->voices = (Voice*)calloc(POLYPHONY, sizeof(Voice));
    s->voice_count = POLYPHONY;
    voice_pool_init(s->voices, POLYPHONY);

    s->demo_step = 0;
    s->demo_timer = 0.0;
    memset(s->demo_notes_on, -1, sizeof(s->demo_notes_on));

    SDL_AudioSpec want = {0};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_F32SYS;
    want.channels = CHANNELS;
    want.samples = BUFFER_SIZE;
    want.callback = audio_callback;
    want.userdata = s;

    s->audio = SDL_OpenAudioDevice(NULL, 0, &want, &s->spec, 0);
    if (s->audio == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed\n");
        exit(1);
    }
    SDL_PauseAudioDevice(s->audio, 0);
}

void synth_close(Synth *s) {
    SDL_CloseAudioDevice(s->audio);
    free(s->voices);
}

void synth_note_on(Synth *s, int midi_note) {
    voice_note_on(s->voices, s->voice_count, midi_note, s->osc_wave, s->osc_detune, s->osc_phase);
}

void synth_note_off(Synth *s, int midi_note) {
    voice_note_off(s->voices, s->voice_count, midi_note);
}