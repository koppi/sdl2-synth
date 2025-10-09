#include "oscilloscope.h"
#include "synth.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if __EMSCRIPTEN__
void oscilloscope_init() {}
void oscilloscope_shutdown() {}
void oscilloscope_update_fft(float sample) {}
void oscilloscope_feed(float sample) {}
void oscilloscope_draw(SDL_Renderer *renderer, const struct Synth *synth,
                       int x, int y, int w, int h, TTF_Font *font) {}
#else
#include <fftw3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WAVEFORM_BUFFER_SIZE 2048

// for waveform
static float waveform_buffer[WAVEFORM_BUFFER_SIZE];
static int waveform_write_pos = 0;
static int buffer_ready = 0;

// for FFT waterfall
static double *in;
static fftw_complex *out;
static fftw_plan fft_plan;
static float waterfall_history[WATERFALL_HEIGHT][FFT_SIZE / 2];
static float waterfall_history[WATERFALL_HEIGHT][FFT_SIZE / 2];
static float hann_window[FFT_SIZE];
static int fft_input_buffer_pos = 0;

void oscilloscope_init() {
    // Create Hann window
    for (int i = 0; i < FFT_SIZE; i++) {
        hann_window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
    }

    // Allocate memory for FFTW arrays
    in = (double*) fftw_malloc(sizeof(double) * FFT_SIZE);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (FFT_SIZE / 2 + 1));
    fft_plan = fftw_plan_dft_r2c_1d(FFT_SIZE, in, out, FFTW_MEASURE);

    // Initialize waterfall history to 0
    memset(waterfall_history, 0, sizeof(waterfall_history));
}

void oscilloscope_shutdown() {
    fftw_destroy_plan(fft_plan);
    fftw_free(in);
    fftw_free(out);
}

void oscilloscope_update_fft(float sample) {
    if (fft_input_buffer_pos < FFT_SIZE) {
        in[fft_input_buffer_pos] = sample * hann_window[fft_input_buffer_pos];
        fft_input_buffer_pos++;
    }

    if (fft_input_buffer_pos == FFT_SIZE) {
        fftw_execute(fft_plan);

        // Scroll waterfall history
        memmove(&waterfall_history[1], &waterfall_history[0], sizeof(float) * (WATERFALL_HEIGHT - 1) * (FFT_SIZE / 2));

        // Calculate magnitudes and store in the first row of history
        for (int i = 0; i < FFT_SIZE / 2; i++) {
            float mag = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
            waterfall_history[0][i] = 20.0f * log10f(mag + 1e-6f);
        }

        // Reset buffer position
        fft_input_buffer_pos = 0;
    }
}

void oscilloscope_feed(float sample) {
  // Clamp sample to reasonable range
  if (sample > 1.0f) sample = 1.0f;
  if (sample < -1.0f) sample = -1.0f;
  
  waveform_buffer[waveform_write_pos] = sample;
  waveform_write_pos = (waveform_write_pos + 1) % WAVEFORM_BUFFER_SIZE;
  buffer_ready = 1;

  }

// Simple draw_text helper
static void draw_text(SDL_Renderer *renderer, const char *text, int x, int y,
                      SDL_Color color, TTF_Font *font) {
  if (!font)
    return;
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

static void get_color_for_magnitude(float mag, Uint8* r, Uint8* g, Uint8* b) {
    float value = (mag + 80.0f) / 80.0f;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    // Simple viridis-like color map
    if (value < 0.25f) {
        *r = 0;
        *g = (Uint8)(255 * (value / 0.25f));
        *b = (Uint8)(255 * (1.0f - (value / 0.25f)));
    } else if (value < 0.5f) {
        *r = 0;
        *g = 255;
        *b = (Uint8)(255 * ((value - 0.25f) / 0.25f));
    } else if (value < 0.75f) {
        *r = (Uint8)(255 * ((value - 0.5f) / 0.25f));
        *g = 255;
        *b = (Uint8)(255 * (1.0f - (value - 0.5f) / 0.25f));
    } else {
        *r = 255;
        *g = (Uint8)(255 * (1.0f - (value - 0.75f) / 0.25f));
        *b = 0;
    }
}

void oscilloscope_draw(SDL_Renderer *renderer, const struct Synth *synth,
                       int x, int y, int w, int h, TTF_Font *font) {
  (void)synth; // Not used in this implementation

  int waveform_h = h / 3;
  int waterfall_y = y + waveform_h;
  int waterfall_h = h - waveform_h;
  
  // Draw oscilloscope background
  SDL_Rect scope_area = {x, y, w, waveform_h};
  SDL_SetRenderDrawColor(renderer, 20, 25, 35, 255);
  SDL_RenderFillRect(renderer, &scope_area);
  
  // Draw grid lines
  SDL_SetRenderDrawColor(renderer, 40, 45, 55, 255);
  for (int i = 1; i < 8; i++) {
    int grid_x = x + (w * i) / 8;
    SDL_RenderDrawLine(renderer, grid_x, y, grid_x, y + waveform_h);
  }
  for (int i = 1; i < 4; i++) {
    int grid_y = y + (waveform_h * i) / 4;
    SDL_RenderDrawLine(renderer, x, grid_y, x + w, grid_y);
  }
  
  // Draw center line (0V reference)
  SDL_SetRenderDrawColor(renderer, 80, 85, 100, 255);
  SDL_RenderDrawLine(renderer, x, y + waveform_h/2, x + w, y + waveform_h/2);
  
  if (buffer_ready) {
      // Find a stable trigger point (zero crossing)
      int trigger_pos = waveform_write_pos;
      int samples_to_display = w > WAVEFORM_BUFFER_SIZE ? WAVEFORM_BUFFER_SIZE : w;
      
      for (int i = 0; i < WAVEFORM_BUFFER_SIZE - samples_to_display; i++) {
        int pos1 = (waveform_write_pos - WAVEFORM_BUFFER_SIZE + i + WAVEFORM_BUFFER_SIZE) % WAVEFORM_BUFFER_SIZE;
        int pos2 = (pos1 + 1) % WAVEFORM_BUFFER_SIZE;
        
        if (waveform_buffer[pos1] <= 0.0f && waveform_buffer[pos2] > 0.0f) {
          trigger_pos = pos2;
          break;
        }
      }
      
      // Draw waveform
      SDL_SetRenderDrawColor(renderer, 255, 220, 40, 255); // Yellow waveform
      for (int px = 0; px < w - 1; px++) {
        int sample_idx1 = (trigger_pos + (px * samples_to_display) / w) % WAVEFORM_BUFFER_SIZE;
        int sample_idx2 = (trigger_pos + ((px + 1) * samples_to_display) / w) % WAVEFORM_BUFFER_SIZE;
        
        float sample1 = waveform_buffer[sample_idx1];
        float sample2 = waveform_buffer[sample_idx2];
        
        int y1 = y + waveform_h/2 - (int)(sample1 * (waveform_h/2 - 10));
        int y2 = y + waveform_h/2 - (int)(sample2 * (waveform_h/2 - 10));
        
        if (y1 < y) y1 = y;
        if (y1 >= y + waveform_h) y1 = y + waveform_h - 1;
        if (y2 < y) y2 = y;
        if (y2 >= y + waveform_h) y2 = y + waveform_h - 1;
        
        SDL_RenderDrawLine(renderer, x + px, y1, x + px + 1, y2);
      }
  }

  // Draw border for waveform
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &scope_area);

  // Draw waterfall
  SDL_Rect waterfall_area = {x, waterfall_y, w, waterfall_h};
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderFillRect(renderer, &waterfall_area);

  for (int row = 0; row < waterfall_h; row++) {
      if (row >= WATERFALL_HEIGHT) break;
      for (int col = 0; col < w; col++) {
          int fft_bin = (int)((float)col / (w - 1) * (FFT_SIZE / 2 - 1));
          float magnitude = waterfall_history[row][fft_bin];

          Uint8 r, g, b;
          get_color_for_magnitude(magnitude, &r, &g, &b);
          
          SDL_SetRenderDrawColor(renderer, r, g, b, 255);
          SDL_RenderDrawPoint(renderer, x + col, waterfall_y + row);
      }
  }

  // Draw border for waterfall
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &waterfall_area);
  
  // Draw labels
  if (font) {
    SDL_Color text_color = {180, 180, 200, 255};
    draw_text(renderer, "Waveform", x + 10, y + 5, text_color, font);
    draw_text(renderer, "Waterfall Spectrogram", x + 10, waterfall_y + 5, text_color, font);

    // Add frequency labels to the waterfall spectrogram
    float nyquist_freq = synth->sample_rate / 2.0f;
    float freq_scale = (float)w / (FFT_SIZE / 2); // Pixels per FFT bin

    // Frequencies to label (Hz)
    float label_freqs[] = {100.0f, 500.0f, 1000.0f, 1048.0f, 5000.0f, 10000.0f, nyquist_freq};
    char label_texts[6][10]; // Buffer for labels

    for (int i = 0; i < sizeof(label_freqs) / sizeof(label_freqs[0]); ++i) {
        float freq = label_freqs[i];
        if (freq > nyquist_freq) continue;

        int fft_bin = (int)(freq / nyquist_freq * (FFT_SIZE / 2));
        int px_x = x + (int)(fft_bin * freq_scale);

        // Avoid drawing labels too close to each other or outside bounds
        if (px_x > x + 20 && px_x < x + w - 20) {
            if (freq >= 1000.0f) {
                snprintf(label_texts[i], sizeof(label_texts[i]), "%.0fkHz", freq / 1000.0f);
            } else {
                snprintf(label_texts[i], sizeof(label_texts[i]), "%.0fHz", freq);
            }
            draw_text(renderer, label_texts[i], px_x, waterfall_y + waterfall_h - 20, text_color, font);
        }
    }
  }
}

#endif
