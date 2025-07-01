#include "synth.h"
#include "osc.h"
#include "voice.h"
#include "effects.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define CHANNELS 2
#define POLYPHONY 64
#define SAMPLE_RATE 48000
#define BUFFER_SIZE 512

static void audio_callback(void *udata, Uint8 *stream, int len);

static int synth_demo_thread(void *ud) {
    Synth *s = (Synth*)ud;
    static const struct {int note, dur_ms;} melody[] = {
        {60,200},{62,200},{64,200},{65,200},{67,200},{69,200},{71,200},{72,400},
        {72,100},{71,100},{69,100},{67,100},{65,100},{64,100},{62,100},{60,400},
        {67,100},{69,100},{71,100},{72,300},{-1,300}
    };
    for (int i = 0; melody[i].note >= 0; ++i) {
        synth_note_on(s, melody[i].note);
        SDL_Delay(melody[i].dur_ms);
        synth_note_off(s, melody[i].note);
    }
    s->demo_playing = 0;
    return 0;
}

void synth_init(Synth *s) {
    memset(s, 0, sizeof(*s));
    s->osc_mode = OSC_SINE | OSC_SQUARE;
    s->volume = 100;
    s->octave = 4;
    s->demo_playing = 0;
    s->flanger_depth = 0.8f;
    s->flanger_rate = 0.18f;
    s->delay_ms = 320.0f;
    s->delay_fb = 0.45f;
    s->reverb_mix = 0.18f;
    s->voices = (Voice*)calloc(POLYPHONY, sizeof(Voice));
    s->voice_count = POLYPHONY;
    voice_pool_init(s->voices, POLYPHONY);

    effects_init(&s->fx, SAMPLE_RATE, BUFFER_SIZE);

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
    effects_free(&s->fx);
}

void synth_note_on(Synth *s, int midi_note) {
    voice_note_on(s->voices, s->voice_count, midi_note, s->osc_mode);
}

void synth_note_off(Synth *s, int midi_note) {
    voice_note_off(s->voices, s->voice_count, midi_note);
}

void synth_play_demo(Synth *s) {
    if (s->demo_playing) return;
    s->demo_playing = 1;
    SDL_CreateThread(synth_demo_thread, "DemoMelody", s);
}

void synth_toggle_osc(Synth *s) {
    // Sine -> Square -> Noise -> Saw -> Sine+Square -> Sine+Noise -> Sine+Saw -> Square+Noise -> Square+Saw -> Noise+Saw -> Sine+Square+Noise -> Sine+Square+Saw -> Sine+Noise+Saw -> Square+Noise+Saw -> All -> Sine
    static const int modes[] = {
        OSC_SINE, OSC_SQUARE, OSC_NOISE, OSC_SAW,
        OSC_SINE|OSC_SQUARE, OSC_SINE|OSC_NOISE, OSC_SINE|OSC_SAW,
        OSC_SQUARE|OSC_NOISE, OSC_SQUARE|OSC_SAW, OSC_NOISE|OSC_SAW,
        OSC_SINE|OSC_SQUARE|OSC_NOISE, OSC_SINE|OSC_SQUARE|OSC_SAW,
        OSC_SINE|OSC_NOISE|OSC_SAW, OSC_SQUARE|OSC_NOISE|OSC_SAW,
        OSC_SINE|OSC_SQUARE|OSC_NOISE|OSC_SAW
    };
    int i;
    for (i=0; i<sizeof(modes)/sizeof(modes[0]); ++i) {
        if (s->osc_mode == modes[i]) break;
    }
    s->osc_mode = modes[(i+1)%((int)(sizeof(modes)/sizeof(modes[0])))];
}

void synth_octave_mod(Synth *s, int delta) {
    s->octave += delta;
    if (s->octave < 1) s->octave = 1;
    if (s->octave > 7) s->octave = 7;
}

void synth_volume_mod(Synth *s, int delta) {
    s->volume += delta*5;
    if (s->volume < 0) s->volume = 0;
    if (s->volume > 127) s->volume = 127;
}

void synth_flanger_mod(Synth *s, int delta) {
    s->flanger_depth += delta*0.05f;
    if (s->flanger_depth < 0.0f) s->flanger_depth = 0.0f;
    if (s->flanger_depth > 1.0f) s->flanger_depth = 1.0f;
}

void synth_delay_mod(Synth *s, int delta) {
    s->delay_ms += delta * 20.0f;
    if (s->delay_ms < 20.0f) s->delay_ms = 20.0f;
    if (s->delay_ms > 2000.0f) s->delay_ms = 2000.0f;
}

void synth_state_string(Synth *s, char *buf, int buflen) {
    const char *osc = "Unknown";
    if (s->osc_mode == OSC_SINE) osc = "Sine";
    else if (s->osc_mode == OSC_SQUARE) osc = "Square";
    else if (s->osc_mode == OSC_NOISE) osc = "Noise";
    else if (s->osc_mode == OSC_SAW) osc = "Saw";
    else if (s->osc_mode == (OSC_SINE | OSC_SQUARE)) osc = "Sine+Square";
    else if (s->osc_mode == (OSC_SINE | OSC_NOISE)) osc = "Sine+Noise";
    else if (s->osc_mode == (OSC_SINE | OSC_SAW)) osc = "Sine+Saw";
    else if (s->osc_mode == (OSC_SQUARE | OSC_NOISE)) osc = "Square+Noise";
    else if (s->osc_mode == (OSC_SQUARE | OSC_SAW)) osc = "Square+Saw";
    else if (s->osc_mode == (OSC_NOISE | OSC_SAW)) osc = "Noise+Saw";
    else if (s->osc_mode == (OSC_SINE | OSC_SQUARE | OSC_NOISE)) osc = "Sine+Square+Noise";
    else if (s->osc_mode == (OSC_SINE | OSC_SQUARE | OSC_SAW)) osc = "Sine+Square+Saw";
    else if (s->osc_mode == (OSC_SINE | OSC_NOISE | OSC_SAW)) osc = "Sine+Noise+Saw";
    else if (s->osc_mode == (OSC_SQUARE | OSC_NOISE | OSC_SAW)) osc = "Square+Noise+Saw";
    else if (s->osc_mode == (OSC_SINE | OSC_SQUARE | OSC_NOISE | OSC_SAW)) osc = "Sine+Square+Noise+Saw";
    snprintf(buf, buflen, "SDL2 Synth | Octave: %d | Volume: %d | Osc: %s | Flanger: %.2f | Delay: %.0fms | Reverb: %.2f",
        s->octave, s->volume, osc, s->flanger_depth, s->delay_ms, s->reverb_mix
    );
}

// Called from SDL audio thread!
static void audio_callback(void *udata, Uint8 *stream, int len) {
    Synth *s = (Synth*)udata;
    float *out = (float*)stream;
    int samples = len / sizeof(float) / CHANNELS;

    memset(out, 0, sizeof(float)*samples*CHANNELS);

    // Mix all voices
    for (int v=0; v<s->voice_count; ++v) {
        voice_render(&s->voices[v], out, samples, s->osc_mode, s->octave, s->volume, SAMPLE_RATE);
    }
    // Effects chain
    effects_process(&s->fx, out, samples, s->flanger_depth, s->flanger_rate, s->delay_ms, s->delay_fb, s->reverb_mix);

    // Clamp
    for (int i=0; i<samples*CHANNELS; ++i) {
        if (out[i]<-1.0f) out[i]=-1.0f;
        if (out[i]>1.0f) out[i]=1.0f;
    }
}