#include "oscilloscope.h"
#include <math.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FFT_SIZE 2048*4

static float oscilloscope_buffer[FFT_SIZE];
static int oscilloscope_pos = 0;

// Forward declaration for text drawing (optional, if you want axis labels)
static void draw_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color color, TTF_Font *font);

static void fft_real(float* data, float* out_re, float* out_im, int n) {
    int i, j, k, m;
    int n2, a;
    float c, s, t1, t2, u1, u2, z;

    // Bit-reversal permutation
    j = 0;
    for (i = 0; i < n - 1; ++i) {
        if (i < j) {
            float tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
        m = n / 2;
        while (m >= 1 && j >= m) {
            j -= m;
            m /= 2;
        }
        j += m;
    }

    // Initialize real/imag
    for (i = 0; i < n; ++i) {
        out_re[i] = data[i];
        out_im[i] = 0.0f;
    }

    // Danielson-Lanczos
    int stages = (int)log2f((float)n);
    for (int s = 1; s <= stages; ++s) {
        m = 1 << s;
        n2 = m / 2;
        float theta = -M_PI / n2;
        float wtemp = sinf(0.5f * theta);
        float wpr = -2.0f * wtemp * wtemp;
        float wpi = sinf(theta);
        float wr = 1.0f, wi = 0.0f;
        for (j = 0; j < n2; ++j) {
            for (k = j; k < n; k += m) {
                a = k + n2;
                t1 = wr * out_re[a] - wi * out_im[a];
                t2 = wr * out_im[a] + wi * out_re[a];
                out_re[a] = out_re[k] - t1;
                out_im[a] = out_im[k] - t2;
                out_re[k] += t1;
                out_im[k] += t2;
            }
            z = wr;
            wr = wr + wr * wpr - wi * wpi;
            wi = wi + wi * wpr + z * wpi;
        }
    }
}

void oscilloscope_feed(float sample) {
    oscilloscope_buffer[oscilloscope_pos] = sample;
    oscilloscope_pos = (oscilloscope_pos + 1) % FFT_SIZE;
}

// Simple draw_text for axis labels (uses SDL_ttf, ensure a font is loaded in your GUI)
static void draw_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color color, TTF_Font *font) {
    if (!font) return;
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            SDL_Rect dst = {x, y, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }
}

void oscilloscope_draw(SDL_Renderer *renderer, const Synth *synth, int x, int y, int w, int h, TTF_Font *font) {
    (void)synth; // Not used, but kept for possible future extension

    // Area split: top 2/3 waveform, bottom 1/3 spectrum
    int waveform_height = (int)(h * 0.6f);
    int spectrum_height = h - waveform_height - 10;
    if (spectrum_height < 30) spectrum_height = 30;

    // --- Draw waveform area ---
    SDL_Rect waveform_area = {x, y, w, waveform_height};
    SDL_SetRenderDrawColor(renderer, 30, 30, 60, 255);
    SDL_RenderFillRect(renderer, &waveform_area);

    // Draw zero line
    SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255);
    SDL_RenderDrawLine(renderer, x, y + waveform_height / 2, x + w, y + waveform_height / 2);

    // Y axis (vertical)
    SDL_RenderDrawLine(renderer, x, y, x, y + waveform_height);

    // Get a copy of the buffer, find zero crossing for stable display
    float tmp[FFT_SIZE];
    int start = oscilloscope_pos;
    for (int i = 0; i < FFT_SIZE; ++i) {
        tmp[i] = oscilloscope_buffer[(start + i) % FFT_SIZE];
    }
    int zero = 0;
    for (int i = 1; i < FFT_SIZE; ++i) {
        if (tmp[i-1] < 0.0f && tmp[i] >= 0.0f) {
            zero = i;
            break;
        }
    }

    // Draw waveform (yellow)
    SDL_SetRenderDrawColor(renderer, 255, 220, 40, 255);
    for (int px = 0; px < w - 1; ++px) {
        int i1 = zero + (px    ) * FFT_SIZE / w;
        int i2 = zero + (px + 1) * FFT_SIZE / w;
        if (i2 >= FFT_SIZE) break;
        int y1 = y + waveform_height / 2 - (int)(tmp[i1] * (waveform_height / 2 - 4));
        int y2 = y + waveform_height / 2 - (int)(tmp[i2] * (waveform_height / 2 - 4));
        SDL_RenderDrawLine(renderer, x + px, y1, x + px + 1, y2);
    }

    // Axis labels for waveform
    SDL_Color label_col = {180, 180, 220, 255};
    draw_text(renderer, "0", x + 4, y + waveform_height / 2 + 2, label_col, font);
    draw_text(renderer, "+1", x + 4, y + 4, label_col, font);
    draw_text(renderer, "-1", x + 4, y + waveform_height - 18, label_col, font);

    // --- FFT and spectrum ---
    float re[FFT_SIZE], im[FFT_SIZE], fft_input[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; ++i) fft_input[i] = tmp[i];
    fft_real(fft_input, re, im, FFT_SIZE);

    // Draw spectrum area
    SDL_Rect spec_area = {x, y + waveform_height + 10, w, spectrum_height};
    SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255);
    SDL_RenderFillRect(renderer, &spec_area);

    // Frequency axis (horizontal)
    SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255);
    SDL_RenderDrawLine(renderer, spec_area.x, spec_area.y + spec_area.h - 1, spec_area.x + spec_area.w, spec_area.y + spec_area.h - 1);

    // Y axis (vertical)
    SDL_RenderDrawLine(renderer, spec_area.x, spec_area.y, spec_area.x, spec_area.y + spec_area.h);

    // Plot spectrum (magnitude, yellow)
    SDL_SetRenderDrawColor(renderer, 255, 220, 40, 255);
    int bins = w;
    for (int px = 0; px < bins; ++px) {
        int bin = px * (FFT_SIZE / 2) / bins;
        float mag = sqrtf(re[bin] * re[bin] + im[bin] * im[bin]) / (FFT_SIZE / 2);
        if (mag > 1.0f) mag = 1.0f;
        int sy = spec_area.y + spec_area.h - (int)(mag * (spec_area.h - 4));
        SDL_RenderDrawLine(renderer, spec_area.x + px, spec_area.y + spec_area.h - 1, spec_area.x + px, sy);
    }

    // Axis labels for spectrum
    draw_text(renderer, "0Hz", spec_area.x + 3, spec_area.y + spec_area.h - 18, label_col, font);
    draw_text(renderer, "Nyquist", spec_area.x + w - 60, spec_area.y + spec_area.h - 18, label_col, font);
    draw_text(renderer, "1.0", spec_area.x + 4, spec_area.y + 4, label_col, font);
    draw_text(renderer, "0", spec_area.x + 4, spec_area.y + spec_area.h - 18, label_col, font);
}
