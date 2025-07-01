#include "gui.h"
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "synth.h"

// --- Constants and helpers ---
#define NUM_KEYS 24
static const int midi_notes[NUM_KEYS] = {
    60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83
};
static const char *key_labels[NUM_KEYS] = {
    "C4","C#","D","D#","E","F","F#","G","G#","A","A#","B",
    "C5","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};
static int is_black(int i) {
    int n = i%12;
    return n==1||n==3||n==6||n==8||n==10;
}
static const char *osc_labels[NUM_OSCS] = {"Osc1", "Osc2", "Osc3", "Osc4"};
static const char *effect_labels[NUM_EFFECT_KNOBS] = {
    "Flanger", "Delay", "Reverb", "Volume"
};

// --- Fonts ---
TTF_Font *font = NULL;
TTF_Font *font_small = NULL;

// --- Dropdown states ---
static int dropdown_open[NUM_OSCS] = {0,0,0,0};

// --- Demo pattern for GUI ---
typedef struct {
    double time;
    int notes[4];
    double length;
    const char* label;
} GuiDemoStep;
static const GuiDemoStep gui_demo_pattern[] = {
    {0.0,  {60, 64, 67, -1}, 0.5, "C"},
    {0.5,  {62, 65, 69, -1}, 0.5, "Dm"},
    {1.0,  {59, 62, 67, -1}, 0.5, "G/B"},
    {1.5,  {60, 64, 67, -1}, 0.5, "C"},
    {2.0,  {65, 69, 72, -1}, 0.5, "F"},
    {2.5,  {64, 67, 71, -1}, 0.5, "Em"},
    {3.0,  {62, 65, 69, -1}, 0.5, "Dm"},
    {3.5,  {60, 64, 67, -1}, 0.5, "C"},
};
#define GUI_DEMO_PATTERN_LEN (sizeof(gui_demo_pattern)/sizeof(gui_demo_pattern[0]))

// --- Font loading/cleanup ---
void gui_init(SDL_Renderer *ren) {
    (void)ren;
    if (!font) font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14);
    if (!font_small) font_small = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 11);
}
void gui_quit() {
    if (font) TTF_CloseFont(font), font = NULL;
    if (font_small) TTF_CloseFont(font_small), font_small = NULL;
}

// --- Modular Knob (flat, modern) ---
static void draw_knob_flat(SDL_Renderer *ren, int x, int y, int radius, float value, float min, float max, const char *label, int active) {
    float norm = (value - min) / (max - min);
    if(norm<0) norm=0; if(norm>1) norm=1;
    float angle = norm * (5.0f*M_PI/4.0f) + (7.0f*M_PI/8.0f);
    int px = x + (int)(radius * 0.7f * cosf(angle));
    int py = y + (int)(radius * 0.7f * sinf(angle));
    Uint8 fill_r=245,fill_g=245,fill_b=245,alpha=255;
    Uint8 border_r=active?250:220, border_g=active?210:220, border_b=active?40:180;
    filledCircleRGBA(ren, x, y, radius, fill_r, fill_g, fill_b, alpha);
    for(int r=radius;r>radius-3;--r)
        circleRGBA(ren, x, y, r, border_r, border_g, border_b, 255);
    thickLineRGBA(ren, x, y, px, py, (radius/8 > 1 ? radius/8 : 2), 255,190,50,255);

    char valstr[16];
    snprintf(valstr, sizeof(valstr), (max>20)?"%.0f":"%.2f", value);
    if (font) {
        SDL_Color c2 = {40,40,40,255};
        SDL_Surface *s = TTF_RenderText_Blended(font, valstr, c2);
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            SDL_Rect dst = {x-s->w/2, y-s->h/2, s->w, s->h};
            SDL_RenderCopy(ren, t, NULL, &dst);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
    if (font_small && label) {
        SDL_Color col = {120,120,100,255};
        SDL_Surface *s = TTF_RenderText_Blended(font_small, label, col);
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            SDL_Rect dst = {x-s->w/2, y+radius+4, s->w, s->h};
            SDL_RenderCopy(ren, t, NULL, &dst);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
}

// --- Waveform icon (used in dropdown/cards) ---
void draw_waveform_icon(SDL_Renderer* ren, int x, int y, int w, int h, int waveform) {
    int px0 = x + 2, py0 = y + h/2;
    int px1, py1;
    for (int i = 1; i < w-4; ++i) {
        float t = (float)i/(w-5);
        float val = 0.0f;
        switch (waveform) {
            case 0: val = 1.0f - 2.0f*t; break; // Saw
            case 1: val = (t < 0.5f) ? 1.0f : -1.0f; break; // Square
            case 2: val = 1.0f-4.0f*fabsf(t-0.5f); break; // Triangle
            case 3: val = sinf(2*M_PI*t); break;
            case 4: val = (t < 0.2f) ? 1.0f : -1.0f; break; // Pulse
            case 5: val = ((rand()%2000)-1000)/1000.0f; break; // Noise
            default: val = 0;
        }
        px1 = x + i;
        py1 = y + h/2 - (int)(val*(h/2-2));
        SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
        SDL_RenderDrawLine(ren, px0, py0, px1, py1);
        px0 = px1; py0 = py1;
    }
}

// --- Oscillator Card ---
static void draw_osc_card(SDL_Renderer *ren, int x, int y, int w, int h, int idx, Synth *synth, int knob_active, int knob_type, int knob_idx, int knob_radius) {
    roundedBoxRGBA(ren, x, y, x+w, y+h, knob_radius, 245,245,245,230);
    roundedRectangleRGBA(ren, x, y, x+w, y+h, knob_radius, 200,200,200,255);

    if(font_small) {
        SDL_Color c = {90,100,120,255};
        SDL_Surface *s = TTF_RenderText_Blended(font_small, osc_labels[idx], c);
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            SDL_Rect dst = {x+10, y+10, s->w, s->h};
            SDL_RenderCopy(ren, t, NULL, &dst);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }

    int box_w = w*0.7, box_h = h/8;
    int wave_x = x + (w-box_w)/2;
    int wave_y = y+box_h+4;

    // Main waveform selector box
    boxRGBA(ren, wave_x, wave_y, wave_x+box_w, wave_y+box_h, 255,240,120,255);
    rectangleRGBA(ren, wave_x, wave_y, wave_x+box_w, wave_y+box_h, 180,160,60,255);
    int icon_cx = wave_x+16, icon_cy = wave_y+box_h/2;
    draw_waveform_icon(ren, icon_cx-12, icon_cy-12, 24, 24, synth->osc_wave[idx]);
    if(font_small) {
        SDL_Color c = {90,90,40,255};
        SDL_Surface *s = TTF_RenderText_Blended(font_small, waveform_names[synth->osc_wave[idx]], c);
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            SDL_Rect dst = {wave_x+32, wave_y+box_h/2-s->h/2, s->w, s->h};
            SDL_RenderCopy(ren, t, NULL, &dst);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
    filledTrigonRGBA(ren, wave_x+box_w-18, wave_y+box_h/2-3, wave_x+box_w-8, wave_y+box_h/2-3, wave_x+box_w-13, wave_y+box_h/2+5, 120,110,60,255);

    // Dropdown
    if (dropdown_open[idx]) {
        for(int i=0; i<NUM_WAVEFORMS; ++i) {
            Uint8 rr=255,gg=240,bb=120,aa=255;
            if (i==synth->osc_wave[idx]) { rr=255;gg=255;bb=170; }
            boxRGBA(ren, wave_x, wave_y+box_h+i*box_h, wave_x+box_w, wave_y+box_h+(i+1)*box_h, rr,gg,bb,aa);
            rectangleRGBA(ren, wave_x, wave_y+box_h+i*box_h, wave_x+box_w, wave_y+box_h+(i+1)*box_h, 180,160,60,255);
            draw_waveform_icon(ren, wave_x+4, wave_y+box_h+4+i*box_h, 20, 20, i);
            if (font_small) {
                SDL_Color c = {90,90,40,255};
                SDL_Surface *s = TTF_RenderText_Blended(font_small, waveform_names[i], c);
                SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
                SDL_Rect dst = {wave_x+28, wave_y+box_h/2-s->h/2 + box_h*(i+1), s->w, s->h};
                SDL_RenderCopy(ren, t, NULL, &dst);
                SDL_DestroyTexture(t);
                SDL_FreeSurface(s);
            }
        }
    }

    // Detune knob
    draw_knob_flat(ren, x + w/2, y + h*0.55, knob_radius, synth->osc_detune[idx], -12, 12, "Detune", knob_active && knob_type==0 && knob_idx==idx);
    // Phase knob
    draw_knob_flat(ren, x + w/2, y + h*0.8, knob_radius, synth->osc_phase[idx], 0, 1, "Phase", knob_active && knob_type==1 && knob_idx==idx);
}

// --- Effects Bar ---
static void draw_effects_bar(SDL_Renderer *ren, int x, int y, int bar_w, int knob_radius, Synth *synth, int knob_active, int knob_type, int knob_idx) {
    int spacing = bar_w / NUM_EFFECT_KNOBS;
    for (int i = 0; i < NUM_EFFECT_KNOBS; ++i) {
        float val=0,min=0,max=1;
        if(i==0) { val=synth->flanger_depth; min=0; max=1; }
        if(i==1) { val=synth->delay_ms; min=0; max=1000; }
        if(i==2) { val=synth->reverb_mix; min=0; max=1; }
        if(i==3) { val=synth->volume; min=0; max=127; }
        int cx = x + i*spacing + spacing/2;
        int cy = y;
        draw_knob_flat(ren, cx, cy, knob_radius, val, min, max, effect_labels[i], knob_active && knob_type==2 && knob_idx==i);
    }
}

// --- Virtual Keyboard ---
static void draw_virtual_keyboard(SDL_Renderer *ren, int x, int y, int key_w, int key_h, int key_states[128]) {
    int black_h = key_h * 0.6, radius = key_w/5 > 4 ? key_w/5 : 4;
    for(int i=0, wi=0; i<NUM_KEYS; ++i) {
        if(!is_black(i)) {
            int kx = x + wi*key_w;
            Uint8 col = key_states[midi_notes[i]] ? 255 : 240;
            Uint8 gcol = key_states[midi_notes[i]] ? 210 : 240;
            boxRGBA(ren, kx, y, kx+key_w-1, y+key_h, col,gcol,120,255);
            roundedRectangleRGBA(ren, kx, y, kx+key_w-1, y+key_h, radius, 190,170,120,255);
            if(font_small) {
                SDL_Color c = {70,70,50,255};
                SDL_Surface *s = TTF_RenderText_Blended(font_small, key_labels[i], c);
                if (s) {
                    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
                    SDL_Rect dst = {kx+key_w/2-s->w/2, y+key_h-18, s->w, s->h};
                    SDL_RenderCopy(ren, t, NULL, &dst);
                    SDL_DestroyTexture(t);
                    SDL_FreeSurface(s);
                }
            }
            wi++;
        }
    }
    for(int i=0, wi=0; i<NUM_KEYS; ++i) {
        if(!is_black(i)) wi++;
        else {
            int kx = x + (wi-1)*key_w + 3*key_w/4;
            Uint8 col = key_states[midi_notes[i]] ? 80 : 24;
            boxRGBA(ren, kx, y, kx+key_w/2, y+black_h, col,col,col,255);
            roundedRectangleRGBA(ren, kx, y, kx+key_w/2, y+black_h, radius, 20,20,20,255);
        }
    }
}

// --- Top Bar ---
static void draw_top_bar(SDL_Renderer *ren, int width, int bar_h, int synth_demo_playing, int synth_demo_step) {
    boxRGBA(ren, 0, 0, width, bar_h, 38,68,140,255);
    if (font) {
        SDL_Color c = {255,255,255,255};
        SDL_Surface *s = TTF_RenderText_Blended(font, "SDL2 Synth", c);
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            SDL_Rect dst = {22, bar_h/3, s->w, s->h};
            SDL_RenderCopy(ren, t, NULL, &dst);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
    int btn_w = bar_h*0.8, btn_h = bar_h*0.7, btn_x = width-btn_w-180, btn_y = (bar_h-btn_h)/2;
    boxRGBA(ren, btn_x, btn_y, btn_x+btn_w, btn_y+btn_h, 255,230,80,255);
    rectangleRGBA(ren, btn_x, btn_y, btn_x+btn_w, btn_y+btn_h, 200,180,80,255);
    if (synth_demo_playing)
        filledTrigonRGBA(ren, btn_x+btn_w/3, btn_y+btn_h/3, btn_x+btn_w*0.7, btn_y+btn_h/2, btn_x+btn_w/3, btn_y+btn_h*0.65, 80,70,30,255);
    else {
        int bar = btn_w/8;
        boxRGBA(ren, btn_x+btn_w/3, btn_y+btn_h/3, btn_x+btn_w/3+bar, btn_y+btn_h*0.65, 80,70,30,255);
        boxRGBA(ren, btn_x+btn_w/2, btn_y+btn_h/3, btn_x+btn_w/2+bar, btn_y+btn_h*0.65, 80,70,30,255);
    }
    if (font_small) {
        SDL_Color c = {60,50,15,255};
        SDL_Surface *s = TTF_RenderText_Blended(font_small, "Demo", c);
        SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
        SDL_Rect dst = {btn_x+btn_w/2-s->w/2, btn_y+btn_h+2, s->w, s->h};
        SDL_RenderCopy(ren, t, NULL, &dst);
        SDL_DestroyTexture(t);
        SDL_FreeSurface(s);
    }
    // Demo pattern (steps)
    int pat_x = width-155, pat_y = bar_h/4;
    for (int i = 0; i < 8; ++i) {
        int bx = pat_x + i*26, by = pat_y;
        Uint8 r = 90, g = 110, b = 140, a = 170;
        if (synth_demo_playing && i == synth_demo_step) { r=255; g=220; b=100; a=210; }
        boxRGBA(ren, bx, by, bx+24, by+24, r,g,b,a);
        roundedRectangleRGBA(ren, bx, by, bx+24, by+24, 6, 60,60,64,255);
        SDL_Color text_col = (synth_demo_playing && i==synth_demo_step) ? (SDL_Color){140,90,40,255} : (SDL_Color){220,220,220,255};
        if (font_small) {
            SDL_Surface *s = TTF_RenderText_Blended(font_small, gui_demo_pattern[i].label, text_col);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
                SDL_Rect dst = {bx+12-s->w/2, by+12-s->h/2, s->w, s->h};
                SDL_RenderCopy(ren, t, NULL, &dst);
                SDL_DestroyTexture(t);
                SDL_FreeSurface(s);
            }
        }
    }
}

// --- Main GUI Draw (Responsive!) ---
void gui_draw(SDL_Renderer *ren, Synth *synth, int knob_active, int knob_type, int knob_idx, int key_states[128]) {
    int win_w = 1000, win_h = 500;
    SDL_GetRendererOutputSize(ren, &win_w, &win_h);

    // Layout constants (proportional, with minimums)
    int bar_h = win_h * 0.10; if (bar_h < 48) bar_h = 48;
    int padding = win_w * 0.04; if (padding < 24) padding = 24;
    int osc_gap = win_w * 0.02; if (osc_gap < 10) osc_gap = 10;
    int osc_w = (win_w - 2*padding - osc_gap*3) / 4;
    int osc_h = win_h * 0.35; if (osc_h < 160) osc_h = 160;
    int osc_y = bar_h + padding/2;
    int knob_radius = osc_w/6 > 16 ? osc_w/6 : 16;

    int eff_bar_w = win_w * 0.34;
    if (eff_bar_w < 340) eff_bar_w = 340;
    int eff_x = win_w - eff_bar_w - padding;
    int eff_y = osc_y + osc_h/2 - knob_radius;
    int eff_knob_radius = knob_radius;

    int kb_w = win_w - 2*padding;
    int kb_x = padding;
    int kb_h = win_h * 0.18; if (kb_h < 60) kb_h = 60;
    int kb_y = win_h - kb_h - padding/3;
    int key_w = kb_w / NUM_KEYS;
    int key_h = kb_h;

    // Background
    SDL_SetRenderDrawColor(ren, 36, 40, 46, 255);
    SDL_RenderClear(ren);

    // Top bar
    draw_top_bar(ren, win_w, bar_h, synth->demo_playing, synth->demo_step);

    // Oscillator Cards
    for (int i = 0; i < NUM_OSCS; ++i) {
        int x = padding + i*(osc_w+osc_gap);
        draw_osc_card(ren, x, osc_y, osc_w, osc_h, i, synth, knob_active, knob_type, knob_idx, knob_radius);
    }

    // Effects bar
    draw_effects_bar(ren, eff_x, eff_y, eff_bar_w, eff_knob_radius, synth, knob_active, knob_type, knob_idx);

    // Keyboard
    draw_virtual_keyboard(ren, kb_x, kb_y, key_w, key_h, key_states);
}

// --- Dropdown handling (osc waveform, responsive!) ---
int gui_handle_waveform_dropdown(SDL_Event *e, Synth *synth) {
    int win_w = 1000, win_h = 500;
    SDL_Window *window = SDL_GetWindowFromID(e->button.windowID);
    if (window) SDL_GetWindowSize(window, &win_w, &win_h);

    int bar_h = win_h * 0.10; if (bar_h < 48) bar_h = 48;
    int padding = win_w * 0.04; if (padding < 24) padding = 24;
    int osc_gap = win_w * 0.02; if (osc_gap < 10) osc_gap = 10;
    int osc_w = (win_w - 2*padding - osc_gap*3) / 4;
    int osc_h = win_h * 0.35; if (osc_h < 160) osc_h = 160;
    int osc_y = bar_h + padding/2;
    int box_w = osc_w*0.7, box_h = osc_h/8;

    if (e->type == SDL_MOUSEBUTTONDOWN) {
        int mx = e->button.x, my = e->button.y;
        for (int i = 0; i < NUM_OSCS; ++i) {
            int x = padding + i*(osc_w+osc_gap);
            int wave_x = x + (osc_w-box_w)/2;
            int wave_y = osc_y+box_h+4;
            // Click on main box
            if(mx >= wave_x && mx < wave_x+box_w && my >= wave_y && my < wave_y+box_h) {
                dropdown_open[i] = !dropdown_open[i];
                for (int j = 0; j < NUM_OSCS; ++j) if(j!=i) dropdown_open[j]=0;
                return 1;
            }
            if(dropdown_open[i]) {
                for(int j=0;j<NUM_WAVEFORMS;++j) {
                    int bx = wave_x, by = wave_y + box_h + j*box_h;
                    if(mx >= bx && mx < bx+box_w && my >= by && my < by+box_h) {
                        synth->osc_wave[i] = j;
                        dropdown_open[i]=0;
                        return 1;
                    }
                }
                if(!(mx >= wave_x && mx < wave_x+box_w && my >= wave_y && my < wave_y+box_h+box_h*NUM_WAVEFORMS))
                    dropdown_open[i]=0;
            }
        }
    }
    return 0;
}