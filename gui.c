#include "gui.h"
#include "synth.h"
#include "midi.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static TTF_Font *font = NULL;

void gui_init(SDL_Renderer *ren) {
    font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
    if (!font) font = TTF_OpenFont("/Library/Fonts/Arial.ttf", 16);
    if (!font) font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 16);
}

void gui_quit() {
    if (font) TTF_CloseFont(font);
    font = NULL;
}

void gui_draw_keyboard(SDL_Renderer *ren, Synth *s) {
    const int key_w = 48, key_h = 160, black_h = 100, black_w = 28;
    const int x0 = 60, y0 = 128;
    // White keys
    for (int k=0; k<20; ++k) {
        int midi = midi_qwertz_map_white[k] + (s->octave-4)*12;
        int x = x0 + k*key_w;
        SDL_Rect r = {x, y0, key_w, key_h};
        int on = 0;
        for (int v=0; v<s->voice_count; ++v)
            if (s->voices[v].active && s->voices[v].midi == midi)
                on = 1;
        SDL_SetRenderDrawColor(ren, on?0:255, on?192:255, on?64:255, 255);
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawColor(ren, 48,48,48,255);
        SDL_RenderDrawRect(ren, &r);
    }
    // Black keys
    for (int k=0; k<20; ++k) {
        int midi = midi_qwertz_map_white[k] + (s->octave-4)*12;
        int black = midi_black_offset(midi%12);
        if (black >= 0) {
            int x = x0 + k*key_w + key_w*3/4;
            SDL_Rect r = {x, y0, black_w, black_h};
            int on = 0;
            for (int v=0; v<s->voice_count; ++v)
                if (s->voices[v].active && s->voices[v].midi == midi+1)
                    on = 1;
            SDL_SetRenderDrawColor(ren, on?16:24, on?8:8, on?32:32, 255);
            SDL_RenderFillRect(ren, &r);
            SDL_SetRenderDrawColor(ren, 0,0,0,255);
            SDL_RenderDrawRect(ren, &r);
        }
    }
    // Draw labels
    if (font) {
        for (int k=0; k<20; ++k) {
            int midi = midi_qwertz_map_white[k];
            int x = x0 + k*key_w + 4, y = y0+key_h+4;
            char txt[8];
            midi_note_name(midi, txt);
            SDL_Color col = {240,240,240,255};
            SDL_Surface *surf = TTF_RenderText_Blended(font, txt, col);
            SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_Rect r = {x, y, surf->w, surf->h};
            SDL_RenderCopy(ren, tex, NULL, &r);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }
}