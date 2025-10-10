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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WAVEFORM_BUFFER_SIZE 2048

// for waveform
static float waveform_buffer[WAVEFORM_BUFFER_SIZE];
static int waveform_write_pos = 0;
static int buffer_ready = 0;

void oscilloscope_init() {
    // Initialize waveform buffer to 0
    memset(waveform_buffer, 0, sizeof(waveform_buffer));
}

void oscilloscope_shutdown() {
}

void oscilloscope_update_fft(float sample) {
    (void)sample;
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

void oscilloscope_draw(SDL_Renderer *renderer, const struct Synth *synth,
                       int x, int y, int w, int h, TTF_Font *font) {
  (void)synth; // Not used in this implementation

  int waveform_h = h; // Use the full height for the waveform
  
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
        
        int y1 = y + waveform_h/2 - (int)(sample1 * (waveform_h/2));
        int y2 = y + waveform_h/2 - (int)(sample2 * (waveform_h/2));
        
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
  
  // Draw labels
  if (font) {
    SDL_Color text_color = {180, 180, 200, 255};
    draw_text(renderer, "Waveform", x + 10, y + 5, text_color, font);
  }
}

#endif