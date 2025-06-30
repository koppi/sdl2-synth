#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "synth.h"
#include "gui.h"
#include "midi.h"

#define WIDTH 960
#define HEIGHT 320

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("SDL2 Synth", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    if (!win) { fprintf(stderr, "Window creation failed\n"); return 1; }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "Renderer creation failed\n"); return 1; }

    Synth synth;
    synth_init(&synth);

    gui_init(ren);

    int running = 1;
    SDL_Event e;
    Uint32 lastTick = SDL_GetTicks();
    while (running) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: running = 0; break;
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    int down = e.type == SDL_KEYDOWN;
                    SDL_Keycode sym = e.key.keysym.sym;

                    // ESC key quits the app
                    if (down && sym == SDLK_ESCAPE) {
                        running = 0;
                        break;
                    }

                    // Melody demo
                    if (down && sym == SDLK_SPACE) synth_play_demo(&synth);

                    // Oscillator toggle
                    if (down && sym == SDLK_F1) synth_toggle_osc(&synth);

                    // Flanger
                    if (down && sym == SDLK_F3) synth_flanger_mod(&synth, -1);
                    if (down && sym == SDLK_F4) synth_flanger_mod(&synth, 1);

                    // Delay
                    if (down && sym == SDLK_F5) synth_delay_mod(&synth, -1);
                    if (down && sym == SDLK_F6) synth_delay_mod(&synth, 1);

                    // Octave
                    if (down && sym == SDLK_UP) synth_octave_mod(&synth, 1);
                    if (down && sym == SDLK_DOWN) synth_octave_mod(&synth, -1);

                    // Volume
                    if (down && sym == SDLK_RIGHT) synth_volume_mod(&synth, 1);
                    if (down && sym == SDLK_LEFT) synth_volume_mod(&synth, -1);

                    // MIDI key mapping
                    int midi = midi_key_to_note(sym, e.key.keysym.mod);
                    if (midi >= 0) {
                        if (down)
                            synth_note_on(&synth, midi);
                        else
                            synth_note_off(&synth, midi);
                    }
                } break;
            }
        }

        // Update window title with state
        char title[256];
        synth_state_string(&synth, title, sizeof(title));
        SDL_SetWindowTitle(win, title);

        // Render GUI
        SDL_SetRenderDrawColor(ren, 32,32,32,255);
        SDL_RenderClear(ren);
        gui_draw_keyboard(ren, &synth);
        SDL_RenderPresent(ren);

        // Cap at 60fps
        Uint32 now = SDL_GetTicks();
        if (now - lastTick < 16) SDL_Delay(16 - (now - lastTick));
        lastTick = now;
    }

    synth_close(&synth);
    gui_quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}