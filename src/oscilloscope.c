#include "oscilloscope.h"
#include "synth.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
 

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

  int waveform_h = h; // Use the full height for waveform

  // Split width for two channels
  int channel_width = w / 2;
  int left_x = x;
  int right_x = x + channel_width + 2; // Add small gap between channels

  // Draw left channel background
  SDL_Rect left_area = {left_x, y, channel_width, waveform_h};
  SDL_SetRenderDrawColor(renderer, 20, 25, 35, 255);
  SDL_RenderFillRect(renderer, &left_area);

  // Draw right channel background
  SDL_Rect right_area = {right_x, y, channel_width, waveform_h};
  SDL_RenderFillRect(renderer, &right_area);

  // Draw grid lines for both channels
  SDL_SetRenderDrawColor(renderer, 40, 45, 55, 255);
  for (int i = 1; i < 8; i++) {
    int grid_x1 = left_x + (channel_width * i) / 8;
    int grid_x2 = right_x + (channel_width * i) / 8;
    SDL_RenderDrawLine(renderer, grid_x1, y, grid_x1, y + waveform_h);
    SDL_RenderDrawLine(renderer, grid_x2, y, grid_x2, y + waveform_h);
  }
  for (int i = 1; i < 4; i++) {
    int grid_y = y + (waveform_h * i) / 4;
    SDL_RenderDrawLine(renderer, left_x, grid_y, left_x + channel_width, grid_y);
    SDL_RenderDrawLine(renderer, right_x, grid_y, right_x + channel_width, grid_y);
  }

  // Draw center lines (0V reference)
  SDL_SetRenderDrawColor(renderer, 80, 85, 100, 255);
  SDL_RenderDrawLine(renderer, left_x, y + waveform_h/2, left_x + channel_width, y + waveform_h/2);
  SDL_RenderDrawLine(renderer, right_x, y + waveform_h/2, right_x + channel_width, y + waveform_h/2);

  if (buffer_ready) {
    // Find a stable trigger point on rising edge zero crossing
    int trigger_pos = waveform_write_pos;
    int samples_to_display = channel_width > WAVEFORM_BUFFER_SIZE ? WAVEFORM_BUFFER_SIZE : channel_width;
    
    // Search backwards from write position for stable trigger
    // We'll look for a zero crossing where signal goes from negative to positive
    int search_range = WAVEFORM_BUFFER_SIZE / 4; // Search through 1/4 of buffer
    int stable_trigger = -1;
    int best_trigger = trigger_pos;
    float min_crossing_diff = 1.0f; // Track smallest crossing amplitude difference
    
    for (int offset = 0; offset < search_range; offset++) {
        int idx1 = (trigger_pos - offset - 1 + WAVEFORM_BUFFER_SIZE) % WAVEFORM_BUFFER_SIZE;
        int idx2 = (trigger_pos - offset + WAVEFORM_BUFFER_SIZE) % WAVEFORM_BUFFER_SIZE;
        
        float sample1 = waveform_buffer[idx1];
        float sample2 = waveform_buffer[idx2];
        
        // Check for rising edge zero crossing (negative to positive)
        if (sample1 < 0.0f && sample2 >= 0.0f) {
            // Calculate how close to zero this crossing is
            float crossing_diff = fabsf(sample1) + fabsf(sample2);
            
            // Prefer crossings with smaller amplitude difference (closer to true zero crossing)
            if (crossing_diff < min_crossing_diff) {
                min_crossing_diff = crossing_diff;
                best_trigger = idx2;
                
                // Additional stability check: ensure signal has some amplitude after crossing
                // Check a few samples ahead to make sure this isn't noise
                int stable_count = 0;
                float amplitude_sum = 0.0f;
                for (int check = 0; check < 8 && check < samples_to_display; check++) {
                    int check_idx = (idx2 + check) % WAVEFORM_BUFFER_SIZE;
                    float check_sample = waveform_buffer[check_idx];
                    amplitude_sum += fabsf(check_sample);
                    if (fabsf(check_sample) > 0.01f) { // Small threshold to avoid noise
                        stable_count++;
                    }
                }
                
                // Consider it stable if at least half the samples have measurable amplitude
                if (stable_count >= 4 && amplitude_sum / 8.0f > 0.02f) {
                    stable_trigger = idx2;
                }
            }
        }
    }
    
    // Use stable trigger if found, otherwise use best zero crossing
    if (stable_trigger >= 0) {
        trigger_pos = stable_trigger;
    } else if (best_trigger != trigger_pos) {
        trigger_pos = best_trigger;
    }

    // Find the actual signal range for auto-scaling
    float min_sample = 0.0f, max_sample = 0.0f;
    int first_sample = 1;
    
    for (int i = 0; i < samples_to_display; i++) {
      int sample_idx = (trigger_pos + i) % WAVEFORM_BUFFER_SIZE;
      float sample = waveform_buffer[sample_idx];
      
      if (first_sample) {
        min_sample = max_sample = sample;
        first_sample = 0;
      } else {
        if (sample < min_sample) min_sample = sample;
        if (sample > max_sample) max_sample = sample;
      }
    }
    
    // Calculate scaling factor to fit the waveform in the available height
    float signal_range = max_sample - min_sample;
    float scale_factor;
    
    if (signal_range < 0.001f) {
      // Signal is essentially flat, use default scaling
      scale_factor = (waveform_h / 2.0f) * 0.8f; // Use 80% of available range
    } else {
      // Scale to fit 90% of available height
      scale_factor = (waveform_h / 2.0f) * 0.9f / (signal_range / 2.0f);
    }
    
    // Calculate center offset
    float signal_center = (max_sample + min_sample) / 2.0f;
    int center_y = y + waveform_h / 2;

    // Draw left channel waveform (red)
    SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255); // Red color
    for (int px = 0; px < channel_width - 1; px++) {
      int sample_idx1 = (trigger_pos + (px * samples_to_display) / channel_width) % WAVEFORM_BUFFER_SIZE;
      int sample_idx2 = (trigger_pos + ((px + 1) * samples_to_display) / channel_width) % WAVEFORM_BUFFER_SIZE;

      float sample1 = waveform_buffer[sample_idx1];
      float sample2 = waveform_buffer[sample_idx2];

      // Apply auto-scaling: center the signal and scale it to fit
      int y1 = center_y - (int)((sample1 - signal_center) * scale_factor);
      int y2 = center_y - (int)((sample2 - signal_center) * scale_factor);

      // Clamp to drawing area
      if (y1 < y) y1 = y;
      if (y1 >= y + waveform_h) y1 = y + waveform_h - 1;
      if (y2 < y) y2 = y;
      if (y2 >= y + waveform_h) y2 = y + waveform_h - 1;

      SDL_RenderDrawLine(renderer, left_x + px, y1, left_x + px + 1, y2);
    }

    // Draw right channel waveform (green)
    SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255); // Green color
    for (int px = 0; px < channel_width - 1; px++) {
      int sample_idx1 = (trigger_pos + (px * samples_to_display) / channel_width) % WAVEFORM_BUFFER_SIZE;
      int sample_idx2 = (trigger_pos + ((px + 1) * samples_to_display) / channel_width) % WAVEFORM_BUFFER_SIZE;

      float sample1 = waveform_buffer[sample_idx1];
      float sample2 = waveform_buffer[sample_idx2];

      // Apply auto-scaling: center the signal and scale it to fit
      int y1 = center_y - (int)((sample1 - signal_center) * scale_factor);
      int y2 = center_y - (int)((sample2 - signal_center) * scale_factor);

      // Clamp to drawing area
      if (y1 < y) y1 = y;
      if (y1 >= y + waveform_h) y1 = y + waveform_h - 1;
      if (y2 < y) y2 = y;
      if (y2 >= y + waveform_h) y2 = y + waveform_h - 1;

      SDL_RenderDrawLine(renderer, right_x + px, y1, right_x + px + 1, y2);
    }
  }

  // Draw borders for both waveforms
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &left_area);
  SDL_RenderDrawRect(renderer, &right_area);

  // Draw labels for stereo channels
  if (font) {
    SDL_Color text_color = {180, 180, 200, 255};
    
    // Left channel label
    SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255); // Red text
    draw_text(renderer, "L", left_x + 5, y + 5, text_color, font);
    
    // Right channel label
    SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255); // Green text
    draw_text(renderer, "R", right_x + 5, y + 5, text_color, font);
  }
}