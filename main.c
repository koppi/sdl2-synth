#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "synth.h"
#include "gui.h"

#define WIDTH 1000
#define HEIGHT 500

Synth synth;
int key_states[128] = {0};

static int is_black(int i) {
    int n = i%12;
    return n==1||n==3||n==6||n==8||n==10;
}

int midi_key_to_note(SDL_Keycode k) {
    static const char *row1 = "zsxdcvgbhnjmq2w3er5t6y7ui9o0p";
    static const int midi1 = 48; // C3
    for (int i = 0; row1[i]; ++i)
        if (k == row1[i]) return midi1+i;
    return -1;
}

int virtual_keyboard_mouse_to_note(int x, int y) {
    int kb_x = 60, kb_y = 380, key_w = 32, key_h = 80, black_h = 48;
    for(int i=0, wi=0; i<24; ++i) {
        if(!is_black(i)) wi++;
        else {
            int bx = kb_x + (wi-1)*key_w + 3*key_w/4;
            if(x >= bx && x < bx+key_w/2 && y >= kb_y && y < kb_y+black_h)
                return 60+i;
        }
    }
    for(int i=0, wi=0; i<24; ++i) {
        if(!is_black(i)) {
            int wx = kb_x + wi*key_w;
            if(x >= wx && x < wx+key_w && y >= kb_y && y < kb_y+key_h)
                return 60+i;
            wi++;
        }
    }
    return -1;
}

// Declare gui_handle_waveform_dropdown
int gui_handle_waveform_dropdown(SDL_Event *e, Synth *synth);

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER);
    TTF_Init();

    SDL_Window *win = SDL_CreateWindow("SDL2 Synth", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    synth_init(&synth);
    gui_init(ren);

    int running = 1, knob_active = 0, knob_type = 0, knob_idx = 0;
    int drag_start_y = 0; float knob_start_val = 0.0f;
    int mouse_note = -1;
    Uint32 last_ticks = SDL_GetTicks();

    while (running) {
        Uint32 now_ticks = SDL_GetTicks();
        double dt = (now_ticks - last_ticks) / 1000.0;
        last_ticks = now_ticks;

        synth_demo_update(&synth, dt); // Advance demo pattern, if playing

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            // handle waveform drop-downs before anything else
            if(gui_handle_waveform_dropdown(&e, &synth))
                continue;

            switch (e.type) {
                case SDL_QUIT: running = 0; break;
                case SDL_MOUSEBUTTONDOWN: {
                    int mx = e.button.x, my = e.button.y;
                    int base_x = 100, base_y = 80, dx = 120;
                    for (int i = 0; i < NUM_OSCS; ++i) {
                        int kx = base_x + i*dx, ky1 = base_y, ky2 = base_y+110;
                        if ((mx-kx)*(mx-kx)+(my-ky1)*(my-ky1) < KNOB_RADIUS*KNOB_RADIUS) {
                            knob_active = 1; knob_type = 0; knob_idx = i;
                            drag_start_y = my; knob_start_val = synth.osc_detune[i];
                        } else if ((mx-kx)*(mx-kx)+(my-ky2)*(my-ky2) < KNOB_RADIUS*KNOB_RADIUS) {
                            knob_active = 1; knob_type = 1; knob_idx = i;
                            drag_start_y = my; knob_start_val = synth.osc_phase[i];
                        }
                    }
                    int eff_base_x = base_x + NUM_OSCS*dx + 70;
                    for (int i = 0; i < NUM_EFFECT_KNOBS; ++i) {
                        int kx = eff_base_x, ky = base_y + i*90;
                        if ((mx-kx)*(mx-kx)+(my-ky)*(my-ky) < KNOB_RADIUS*KNOB_RADIUS) {
                            knob_active = 1; knob_type = 2; knob_idx = i;
                            drag_start_y = my;
                            if(i==0) knob_start_val = synth.flanger_depth;
                            if(i==1) knob_start_val = synth.delay_ms;
                            if(i==2) knob_start_val = synth.reverb_mix;
                            if(i==3) knob_start_val = synth.volume;
                        }
                    }
                    int n = virtual_keyboard_mouse_to_note(mx, my);
                    if(n >= 0 && n < 128) {
                        synth_note_on(&synth, n);
                        key_states[n] = 1;
                        mouse_note = n;
                    }
                } break;
                case SDL_MOUSEBUTTONUP:
                    knob_active = 0;
                    if(mouse_note >= 0) {
                        synth_note_off(&synth, mouse_note);
                        key_states[mouse_note] = 0;
                        mouse_note = -1;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (knob_active) {
                        int delta = drag_start_y - e.motion.y;
                        if (knob_type==0) {
                            float v = knob_start_val + (delta*0.05f);
                            if (v < -12) v = -12;
                            if (v > 12) v = 12;
                            synth.osc_detune[knob_idx] = v;
                        } else if (knob_type==1) {
                            float v = knob_start_val + (delta*0.005f);
                            if (v < 0) v = 0;
                            if (v > 1) v = 1;
                            synth.osc_phase[knob_idx] = v;
                        } else if (knob_type==2) {
                            if(knob_idx==0) {
                                float v = knob_start_val + delta*0.005f;
                                if(v<0) v=0;
                                if(v>1) v=1;
                                synth.flanger_depth = v;
                            }
                            if(knob_idx==1) {
                                float v = knob_start_val + delta*2.0f;
                                if(v<0) v=0;
                                if(v>1000) v=1000;
                                synth.delay_ms = v;
                            }
                            if(knob_idx==2) {
                                float v = knob_start_val + delta*0.005f;
                                if(v<0) v=0;
                                if(v>1) v=1;
                                synth.reverb_mix = v;
                            }
                            if(knob_idx==3) {
                                float v = knob_start_val + delta*0.5f;
                                if(v<0) v=0;
                                if(v>127) v=127;
                                synth.volume = v;
                            }
                        }
                    }
                    break;
                case SDL_KEYDOWN: {
                    if (e.key.keysym.sym == SDLK_ESCAPE) running = 0;
                    if (e.key.keysym.sym == SDLK_F5 || e.key.keysym.sym == SDLK_SPACE) { // Toggle demo music with F5 or Space
                        synth.demo_playing = !synth.demo_playing;
                        if (!synth.demo_playing) {
                            for (int i = 0; i < 8; ++i) {
                                if (synth.demo_notes_on[i] >= 0) {
                                    synth_note_off(&synth, synth.demo_notes_on[i]);
                                    synth.demo_notes_on[i] = -1;
                                }
                            }
                            synth.demo_step = 0;
                            synth.demo_timer = 0.0;
                        }
                    }
                    int midi = midi_key_to_note(e.key.keysym.sym);
                    if (midi >= 0) {
                        synth_note_on(&synth, midi);
                        key_states[midi] = 1;
                    }
                } break;
                case SDL_KEYUP: {
                    int midi = midi_key_to_note(e.key.keysym.sym);
                    if (midi >= 0) {
                        synth_note_off(&synth, midi);
                        key_states[midi] = 0;
                    }
                } break;
            }
        }
        gui_draw(ren, &synth, knob_active, knob_type, knob_idx, key_states);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }
    synth_close(&synth);
    gui_quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}