#include "gui.h"
#include "oscilloscope.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dejavusans_ttf.h" // Include the embedded font data
#include "microui.h" // Include microui header

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static TTF_Font *gui_font = NULL;
static mu_Context mu_ctx;
static SDL_Renderer *renderer = NULL;

// Keyboard state
static int pressed_keys[128] = {0}; // 0: not pressed, 1: pressed

// microui SDL2 renderer implementation
static void flush(void) {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
}

static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
  SDL_Rect rect = { dst.x, dst.y, dst.w, dst.h };
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

static void draw_rect(mu_Rect rect, mu_Color color) {
  push_quad(rect, mu_rect(0, 0, 0, 0), color);
}

static void draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
  if (!gui_font || !text) return;
  
  SDL_Color sdl_color = {color.r, color.g, color.b, color.a};
  SDL_Surface *surf = TTF_RenderText_Blended(gui_font, text, sdl_color);
  if (surf) {
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
      SDL_Rect dst = {pos.x, pos.y, surf->w, surf->h};
      SDL_RenderCopy(renderer, tex, NULL, &dst);
      SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
  }
}

static int text_width(const char *text) {
  if (!gui_font || !text) return 0;
  int w = 0;
  TTF_SizeText(gui_font, text, &w, NULL);
  return w;
}

static int text_height(void) {
  if (!gui_font) return 14;
  return TTF_FontHeight(gui_font);
}

static void r_init(void) {
  // Initialize renderer - no atlas needed for basic implementation
}

static void r_begin(void) {
  flush();
}

static void r_end(void) {
  SDL_RenderPresent(renderer);
}

static void r_push_clip_rect(mu_Rect rect) {
  SDL_Rect clip = {rect.x, rect.y, rect.w, rect.h};
  // SDL_RenderSetClipRect(renderer, &clip);
}

static void r_pop_clip_rect(void) {
  // SDL_RenderSetClipRect(renderer, NULL);
}

static void r_draw_rect(mu_Rect rect, mu_Color color) {
  draw_rect(rect, color);
}

static void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
  draw_text(text, pos, color);
}

static void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
  if (id == MU_ICON_CLOSE) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    // Draw a simple 'X'
    SDL_RenderDrawLine(renderer, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    SDL_RenderDrawLine(renderer, rect.x + rect.w, rect.y, rect.x, rect.y + rect.h);
  } else {
    // Simple icon drawing - just draw a filled rect for other icons
    draw_rect(rect, color);
  }
}

static int r_get_text_width(mu_Font font, const char *text, int len) {
  (void)font; // Unused
  if (len == -1) return text_width(text);
  
  char buffer[256];
  int text_len = strlen(text);
  len = len < text_len ? len : text_len;
  len = len < 255 ? len : 255;
  memcpy(buffer, text, len);
  buffer[len] = '\0';
  return text_width(buffer);
}

static int r_get_text_height(mu_Font font) {
  (void)font; // Unused
  return text_height();
}

static void r_set_clip_rect(mu_Rect rect) {
  r_push_clip_rect(rect);
}

static const char* waveform_options[] = {
  "SINE", "SAW", "SQUARE", "TRI", "NOISE"
};

// Custom combo box implementation using microui popups
static int mu_combo_box(mu_Context *ctx, const char **items, int item_count, int *selected_idx, const char *popup_name) {
  int result = 0;
  
  // Display current selection as a button
  char button_text[64];
  snprintf(button_text, sizeof(button_text), "%s (v)", items[*selected_idx]);
  
  if (mu_button(ctx, button_text)) {
    mu_open_popup(ctx, popup_name);
  }
  
  // Handle popup with options
  if (mu_begin_popup(ctx, popup_name)) {
    for (int i = 0; i < item_count; i++) {
      if (mu_button(ctx, items[i])) {
        *selected_idx = i;
        result = 1; // Signal that selection changed
      }
    }
    mu_end_popup(ctx);
  }
  
  return result;
}

// Helper to check if a MIDI note is a black key
static int is_black_key(int midi_note) {
    int note_in_octave = midi_note % 12;
    return (note_in_octave == 1 || note_in_octave == 3 ||
            note_in_octave == 6 || note_in_octave == 8 ||
            note_in_octave == 10);
}

// Get waveform name for display
static const char* get_waveform_name(OscWaveform wf) {
  switch (wf) {
    case OSC_SINE: return "SINE";
    case OSC_SAW: return "SAW";
    case OSC_SQUARE: return "SQR";
    case OSC_TRI: return "TRI";
    case OSC_NOISE: return "NOISE";
    default: return "???";
  }
}

// Get parameter description for tooltips/help
static const char* get_param_description(const char* param) {
  if (strcmp(param, "pitch") == 0) return "Pitch offset in semitones (-24 to +24)";
  if (strcmp(param, "detune") == 0) return "Fine tuning in semitones (-1 to +1)";
  if (strcmp(param, "gain") == 0) return "Output amplitude (0 to 1)";
  if (strcmp(param, "phase") == 0) return "Phase offset (0-360 deg)";
  if (strcmp(param, "pulse_width") == 0) return "Square wave duty cycle (10% to 90%)";
  if (strcmp(param, "unison_voices") == 0) return "Number of unison voices (1 to 8)";
  if (strcmp(param, "unison_detune") == 0) return "Unison spread amount (0 to 1)";
  return "Parameter control";
}


void gui_init(Gui *gui, SDL_Renderer *sdl_renderer, Synth *synth) {
  gui->renderer = sdl_renderer;
  gui->synth = synth;
  gui->selected_control = -1;
  renderer = sdl_renderer;

  if (TTF_WasInit() == 0)
    TTF_Init();

  // Load font from embedded data
  SDL_RWops *rw = SDL_RWFromConstMem(DejaVuSans_ttf, DejaVuSans_ttf_len);
  if (rw) {
    gui_font = TTF_OpenFontRW(rw, 1, 14); // 1 for close_rw, 14 for font size
    if (!gui_font) {
      fprintf(stderr, "Failed to load embedded font: %s\n", TTF_GetError());
    }
  } else {
    fprintf(stderr, "Failed to create RWops from embedded font data\n");
  }

  // Initialize microui
  mu_init(&mu_ctx);
  mu_ctx.style->padding = 10; // Increased padding
  mu_ctx.style->spacing = 10; // Increased spacing
  
  // Set up microui callbacks
  mu_ctx.text_width = r_get_text_width;
  mu_ctx.text_height = r_get_text_height;
  
  r_init();
}

void gui_shutdown(Gui *gui) {
  (void)gui; // Silence unused parameter warning
  if (gui_font) {
    TTF_CloseFont(gui_font);
    gui_font = NULL;
  }
  if (TTF_WasInit())
    TTF_Quit();
}

TTF_Font* gui_get_font() {
  return gui_font;
}

void gui_set_key_pressed(int midi_note, int is_pressed) {
    if (midi_note >= 0 && midi_note < 128) {
        pressed_keys[midi_note] = is_pressed;
    }
}

// Draw the MIDI keyboard using SDL directly (keep this part as microui isn't ideal for piano keys)
static void draw_keyboard(SDL_Renderer *r, Synth *s, int x, int y, int width, int height) {
    int white_key_width = width / (3 * 7); // 3 octaves, 7 white keys per octave
    int white_key_height = height;
    int black_key_width = white_key_width * 2 / 3;
    int black_key_height = height * 2 / 3;

    // Draw white keys
    int white_key_x_offset = 0;
    for (int i = 0; i < 3; ++i) { // Three octaves
        for (int j = 0; j < 7; ++j) { // 7 white keys per octave
            int midi_note = 48 + i * 12 + (j == 0 ? 0 : (j == 1 ? 2 : (j == 2 ? 4 : (j == 3 ? 5 : (j == 4 ? 7 : (j == 5 ? 9 : 11))))));
            SDL_Rect key_rect = {x + white_key_x_offset, y, white_key_width, white_key_height};

            if (pressed_keys[midi_note]) {
                SDL_SetRenderDrawColor(r, 200, 200, 100, 255); // Pressed color
            } else {
                int is_arp_held = 0;
                for (int k = 0; k < s->arp.held_count; ++k) {
                    if (s->arp.held_notes[k] == midi_note) {
                        is_arp_held = 1;
                        break;
                    }
                }
                if (is_arp_held) {
                    SDL_SetRenderDrawColor(r, 50, 200, 50, 255); // Arpeggiator held color
                } else {
                    SDL_SetRenderDrawColor(r, 255, 255, 255, 255); // White key color
                }
            }
            SDL_RenderFillRect(r, &key_rect);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 255); // Black border
            SDL_RenderDrawRect(r, &key_rect);

            white_key_x_offset += white_key_width;
        }
    }

    // Draw black keys
    white_key_x_offset = 0;
    for (int i = 0; i < 3; ++i) { // Three octaves
        for (int j = 0; j < 7; ++j) {
            int midi_note_base = 48 + i * 12;
            int current_midi_note = -1;
            int black_key_draw_offset = 0;

            if (j == 0) { // C#
                current_midi_note = midi_note_base + 1;
                black_key_draw_offset = white_key_width - black_key_width / 2;
            } else if (j == 1) { // D#
                current_midi_note = midi_note_base + 3;
                black_key_draw_offset = white_key_width * 2 - black_key_width / 2;
            } else if (j == 3) { // F#
                current_midi_note = midi_note_base + 6;
                black_key_draw_offset = white_key_width * 4 - black_key_width / 2;
            } else if (j == 4) { // G#
                current_midi_note = midi_note_base + 8;
                black_key_draw_offset = white_key_width * 5 - black_key_width / 2;
            } else if (j == 5) { // A#
                current_midi_note = midi_note_base + 10;
                black_key_draw_offset = white_key_width * 6 - black_key_width / 2;
            }

            if (current_midi_note != -1) {
                SDL_Rect key_rect = {x + white_key_x_offset + black_key_draw_offset, y, black_key_width, black_key_height};

                if (pressed_keys[current_midi_note]) {
                    SDL_SetRenderDrawColor(r, 100, 100, 50, 255); // Pressed color
                } else {
                    int is_arp_held = 0;
                    for (int k = 0; k < s->arp.held_count; ++k) {
                        if (s->arp.held_notes[k] == current_midi_note) {
                            is_arp_held = 1;
                            break;
                        }
                    }
                    if (is_arp_held) {
                        SDL_SetRenderDrawColor(r, 0, 150, 0, 255); // Arpeggiator held color
                    } else {
                        SDL_SetRenderDrawColor(r, 0, 0, 0, 255); // Black key color
                    }
                }
                SDL_RenderFillRect(r, &key_rect);
                SDL_SetRenderDrawColor(r, 50, 50, 50, 255);
                SDL_RenderDrawRect(r, &key_rect);
            }
            if (!is_black_key(midi_note_base + (j == 0 ? 0 : (j == 1 ? 2 : (j == 2 ? 4 : (j == 3 ? 5 : (j == 4 ? 7 : (j == 5 ? 9 : 11)))))))) {
                white_key_x_offset += white_key_width;
            }
        }
    }
}

// Get MIDI note from mouse coordinates (keyboard interaction)
static int get_midi_note_from_coords(int mx, int my, int keyboard_x, int keyboard_y, int keyboard_width, int keyboard_height) {
    if (mx < keyboard_x || mx >= keyboard_x + keyboard_width ||
        my < keyboard_y || my >= keyboard_y + keyboard_height) {
        return -1; // Not on keyboard
    }

    int white_key_width = keyboard_width / (3 * 7);
    int white_key_height = keyboard_height;
    int black_key_width = white_key_width * 2 / 3;
    int black_key_height = keyboard_height * 2 / 3;

    // Check black keys first (they are on top)
    int white_key_x_offset = 0;
    for (int i = 0; i < 3; ++i) { // Three octaves
        for (int j = 0; j < 7; ++j) { // 7 white keys per octave
            int midi_note_base = 48 + i * 12;
            int current_midi_note = -1;
            int black_key_draw_offset = 0;

            if (j == 0) { // C#
                current_midi_note = midi_note_base + 1;
                black_key_draw_offset = white_key_width - black_key_width / 2;
            } else if (j == 1) { // D#
                current_midi_note = midi_note_base + 3;
                black_key_draw_offset = white_key_width * 2 - black_key_width / 2;
            } else if (j == 3) { // F#
                current_midi_note = midi_note_base + 6;
                black_key_draw_offset = white_key_width * 4 - black_key_width / 2;
            } else if (j == 4) { // G#
                current_midi_note = midi_note_base + 8;
                black_key_draw_offset = white_key_width * 5 - black_key_width / 2;
            } else if (j == 5) { // A#
                current_midi_note = midi_note_base + 10;
                black_key_draw_offset = white_key_width * 6 - black_key_width / 2;
            }

            if (current_midi_note != -1) {
                SDL_Rect key_rect = {keyboard_x + white_key_x_offset + black_key_draw_offset, keyboard_y, black_key_width, black_key_height};
                if (mx >= key_rect.x && mx < key_rect.x + key_rect.w &&
                    my >= key_rect.y && my < key_rect.y + key_rect.h) {
                    return current_midi_note;
                }
            }
            if (!is_black_key(midi_note_base + (j == 0 ? 0 : (j == 1 ? 2 : (j == 2 ? 4 : (j == 3 ? 5 : (j == 4 ? 7 : (j == 5 ? 9 : 11)))))))) {
                white_key_x_offset += white_key_width;
            }
        }
    }

    // Check white keys
    white_key_x_offset = 0;
    for (int i = 0; i < 3; ++i) { // Three octaves
        for (int j = 0; j < 7; ++j) { // 7 white keys per octave
            int midi_note = 48 + i * 12 + (j == 0 ? 0 : (j == 1 ? 2 : (j == 2 ? 4 : (j == 3 ? 5 : (j == 4 ? 7 : (j == 5 ? 9 : 11))))));
            SDL_Rect key_rect = {keyboard_x + white_key_x_offset, keyboard_y, white_key_width, white_key_height};
            if (mx >= key_rect.x && mx < key_rect.x + key_rect.w &&
                my >= key_rect.y && my < key_rect.y + key_rect.h) {
                return midi_note;
            }
            white_key_x_offset += white_key_width;
        }
    }
    return -1;
}

void gui_handle_event(Gui *gui, const SDL_Event *e) {
  static int current_midi_note_on = -1; // Track the currently pressed MIDI note

  // Handle keyboard interaction first (SDL direct)
  if (e->type == SDL_MOUSEBUTTONDOWN) {
    int mx = e->button.x, my = e->button.y;
    
    // Get window dimensions
    int window_width, window_height;
    SDL_Window *w = SDL_GetWindowFromID(1);
    if (w) {
      SDL_GetWindowSize(w, &window_width, &window_height);
    } else {
      window_width = 1280; window_height = 720; // fallback
    }
    
    // Keyboard dimensions
    int keyboard_width = window_width;
    int keyboard_height = window_height * 0.075f; 
    int keyboard_x = 0;
    int osc_h_px = window_height * 0.2f;
    int osc_y_px = window_height - osc_h_px;
    int keyboard_y = osc_y_px - keyboard_height;

    // Check for keyboard interaction
    int midi_note = get_midi_note_from_coords(mx, my, keyboard_x, keyboard_y, keyboard_width, keyboard_height);
    if (midi_note != -1) {
        synth_note_on(gui->synth, midi_note, 1.0f);
        pressed_keys[midi_note] = 1;
        current_midi_note_on = midi_note;
        return; // Event handled by keyboard
    }
  } else if (e->type == SDL_MOUSEBUTTONUP) {
    if (current_midi_note_on != -1) {
        synth_note_off(gui->synth, current_midi_note_on);
        pressed_keys[current_midi_note_on] = 0;
        current_midi_note_on = -1;
    }
  }

  // Convert SDL events to microui events
  switch (e->type) {
    case SDL_MOUSEMOTION:
      mu_input_mousemove(&mu_ctx, e->motion.x, e->motion.y);
      break;
    case SDL_MOUSEBUTTONDOWN:
      mu_input_mousedown(&mu_ctx, e->button.x, e->button.y, e->button.button);
      break;
    case SDL_MOUSEBUTTONUP:
      mu_input_mouseup(&mu_ctx, e->button.x, e->button.y, e->button.button);
      break;
    case SDL_MOUSEWHEEL:
      mu_input_scroll(&mu_ctx, e->wheel.x * -30, e->wheel.y * -30);
      break;
    case SDL_TEXTINPUT:
      mu_input_text(&mu_ctx, e->text.text);
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      {
        int key = 0;
        switch (e->key.keysym.sym) {
          case SDLK_LEFT:      key = MU_KEY_SHIFT; break; // Repurpose existing keys
          case SDLK_RIGHT:     key = MU_KEY_CTRL; break;
          case SDLK_RETURN:    key = MU_KEY_RETURN; break;
          case SDLK_BACKSPACE: key = MU_KEY_BACKSPACE; break;
        }
        if (key && e->type == SDL_KEYDOWN) { mu_input_keydown(&mu_ctx, key); }
        if (key && e->type == SDL_KEYUP)   { mu_input_keyup(&mu_ctx, key); }

        if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_F10) {
          synth_save_default_config(gui->synth);
        } else if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_F11) {
          synth_load_default_config(gui->synth);
        }
      }
      break;
  }
}

void gui_update(Gui *gui) {
  (void)gui; // Unused in microui version
}

// Process microui commands and render
static void render_commands(void) {
  mu_Command *cmd = NULL;
  while (mu_next_command(&mu_ctx, &cmd)) {
    switch (cmd->type) {
      case MU_COMMAND_TEXT: 
        draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
        break;
      case MU_COMMAND_RECT: 
        draw_rect(cmd->rect.rect, cmd->rect.color);
        break;
      case MU_COMMAND_ICON: 
        r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
        break;
      case MU_COMMAND_CLIP: 
        r_set_clip_rect(cmd->clip.rect);
        break;
    }
  }
}

void gui_draw(Gui *gui) {
  SDL_Renderer *r = gui->renderer;
  Synth *s = gui->synth;

  // Get window dimensions
  int window_width, window_height;
  SDL_Window *w = SDL_GetWindowFromID(1);
  if (w) {
    SDL_GetWindowSize(w, &window_width, &window_height);
  } else {
    window_width = 1280; window_height = 720; // fallback
  }

  // Begin microui frame
  mu_begin(&mu_ctx);

  // Main synth window
  if (mu_begin_window_ex(&mu_ctx, "Synth", mu_rect(10, 10, window_width - 20, window_height - 150), MU_OPT_NOCLOSE | MU_OPT_NOTITLE)) {
    
    // Oscillators section with comprehensive slider controls
    // if (mu_header_ex(&mu_ctx, "Oscillators - Complete Slider Interface", MU_OPT_EXPANDED)) {
      mu_layout_row(&mu_ctx, 2, (int[]){ -1, -1 }, 0);
      
      for (int i = 0; i < 6; ++i) { // Loop for all 6 oscillators
        char osc_name[16];
        if (i == 4) snprintf(osc_name, sizeof(osc_name), "BASS");
        else if (i == 5) snprintf(osc_name, sizeof(osc_name), "PERC");
        else snprintf(osc_name, sizeof(osc_name), "OSC %d", i + 1);
        
        mu_begin_panel_ex(&mu_ctx, osc_name, 0);
        
        mu_layout_row(&mu_ctx, 4, (int[]){ 50, 100, 50, 100 }, 25);
        mu_label(&mu_ctx, "Wave:");
        int waveform_idx = (int)s->osc[i].waveform;
        char waveform_popup[32];
        snprintf(waveform_popup, sizeof(waveform_popup), "waveform_popup_%d", i);
        if (mu_combo_box(&mu_ctx, waveform_options, 5, &waveform_idx, waveform_popup)) {
          osc_set_param(&s->osc[i], "waveform", (float)waveform_idx);
        }
        s->osc[i].waveform = (OscWaveform)waveform_idx; // Update the actual oscillator waveform
        mu_label(&mu_ctx, "Pitch:");
        if (mu_slider(&mu_ctx, &s->osc[i].pitch, -24.0f, 24.0f)) {
          osc_set_param(&s->osc[i], "pitch", s->osc[i].pitch);
        }
        
        // Row 2: Unison and Pulse Width
        mu_layout_row(&mu_ctx, 4, (int[]){ 50, 80, 50, 80 }, 25);
        mu_label(&mu_ctx, "Unison:");
        float unison_val = (float)s->osc[i].unison_voices;
        if (mu_slider(&mu_ctx, &unison_val, 1, 8)) {
          osc_set_param(&s->osc[i], "unison_voices", (int)unison_val);
        }
        mu_label(&mu_ctx, "PulseW:");
        if (mu_slider(&mu_ctx, &s->osc[i].pulse_width, 0.0f, 1.0f)) {
          osc_set_param(&s->osc[i], "pulse_width", s->osc[i].pulse_width);
        }
        
        // Row 3: Detune and Gain
        mu_layout_row(&mu_ctx, 4, (int[]){ 50, 80, 50, 80 }, 25);
        mu_label(&mu_ctx, "Detune:");
        if (mu_slider(&mu_ctx, &s->osc[i].detune, -1.0f, 1.0f)) {
          osc_set_param(&s->osc[i], "detune", s->osc[i].detune);
        }
        mu_label(&mu_ctx, "Gain:");
        if (mu_slider(&mu_ctx, &s->osc[i].gain, 0.0f, 1.0f)) {
          osc_set_param(&s->osc[i], "gain", s->osc[i].gain);
        }
        
        // Row 4: Phase and UniSpread
        mu_layout_row(&mu_ctx, 4, (int[]){ 50, 80, 50, 80 }, 25);
        mu_label(&mu_ctx, "Phase:");
        if (mu_slider(&mu_ctx, &s->osc[i].phase, 0.0f, 1.0f)) {
          osc_set_param(&s->osc[i], "phase", s->osc[i].phase);
        }
        mu_label(&mu_ctx, "UniSpread:");
        if (mu_slider(&mu_ctx, &s->osc[i].unison_detune, 0.0f, 1.0f)) {
          osc_set_param(&s->osc[i], "unison_detune", s->osc[i].unison_detune);
        }
        
        // Row 5: Combined display of all parameters
        mu_layout_row(&mu_ctx, 1, (int[]){ -1 }, 0);
        char param_display[256]; // Increased buffer size
        snprintf(param_display, sizeof(param_display), 
                 "Wave:%s P:%.1f D:%.2f G:%.2f (phi):%.0f(deg) PW:%.0f%% U:%d/%.2f", 
                 get_waveform_name(s->osc[i].waveform), s->osc[i].pitch, s->osc[i].detune, s->osc[i].gain,
                 s->osc[i].phase * 360.0f, s->osc[i].pulse_width * 100.0f, 
                 (int)s->osc[i].unison_voices, s->osc[i].unison_detune);
        mu_end_panel(&mu_ctx);
      }
    // } // This '}' closes the Oscillators header if block
    
    // Effects section
    // if (mu_header_ex(&mu_ctx, "Effects", MU_OPT_EXPANDED)) {
      mu_layout_row(&mu_ctx, 3, (int[]){ 150, 150, 150 }, 18);
      
      // Flanger
      mu_begin_panel_ex(&mu_ctx, "Flanger", 0);
      mu_layout_row(&mu_ctx, 2, (int[]){ 60, 100 }, 18);
      mu_label(&mu_ctx, "Depth:");
      if (mu_slider(&mu_ctx, &s->fx.flanger_depth, 0.0f, 1.0f)) {
        fx_set_param(&s->fx, "flanger.depth", s->fx.flanger_depth);
      }
      mu_label(&mu_ctx, "Rate:");
      if (mu_slider(&mu_ctx, &s->fx.flanger_rate, 0.0f, 5.0f)) {
        fx_set_param(&s->fx, "flanger.rate", s->fx.flanger_rate);
      }
      mu_label(&mu_ctx, "Feedback:");
      if (mu_slider(&mu_ctx, &s->fx.flanger_feedback, 0.0f, 1.0f)) {
        fx_set_param(&s->fx, "flanger.feedback", s->fx.flanger_feedback);
      }
      mu_end_panel(&mu_ctx);
      
      // Delay
      mu_begin_panel_ex(&mu_ctx, "Delay", 0);
      mu_layout_row(&mu_ctx, 2, (int[]){ 60, 100 }, 0);
      mu_label(&mu_ctx, "Time:");
      if (mu_slider(&mu_ctx, &s->fx.delay_time, 0.0f, 1.0f)) {
        fx_set_param(&s->fx, "delay.time", s->fx.delay_time);
      }
      mu_label(&mu_ctx, "Feedback:");
      if (mu_slider(&mu_ctx, &s->fx.delay_feedback, 0.0f, 1.0f)) {
        fx_set_param(&s->fx, "delay.feedback", s->fx.delay_feedback);
      }
      mu_label(&mu_ctx, "Mix:");
      if (mu_slider(&mu_ctx, &s->fx.delay_mix, 0.0f, 1.0f)) {
        fx_set_param(&s->fx, "delay.mix", s->fx.delay_mix);
      }
      mu_end_panel(&mu_ctx);
      
      // Reverb
      mu_begin_panel_ex(&mu_ctx, "Reverb", 0);
      mu_layout_row(&mu_ctx, 2, (int[]){ 60, 100 }, 0);
      mu_label(&mu_ctx, "Size:");
      if (mu_slider(&mu_ctx, &s->fx.reverb_size, 0.0f, 1.0f)) {
        fx_set_param(&s->fx, "reverb.size", s->fx.reverb_size);
      }
      mu_label(&mu_ctx, "Mix:");
      if (mu_slider(&mu_ctx, &s->fx.reverb_mix, 0.0f, 1.0f)) {
        fx_set_param(&s->fx, "reverb.mix", s->fx.reverb_mix);
      }
      mu_label(&mu_ctx, "Damping:");
      if (mu_slider(&mu_ctx, &s->fx.reverb_damping, 0.0f, 1.0f)) {
        fx_set_param(&s->fx, "reverb.damping", s->fx.reverb_damping);
      }
      mu_end_panel(&mu_ctx);
    // } // This '}' closes the Effects header if block
    
    // Mixer section
    // if (mu_header_ex(&mu_ctx, "Mixer", MU_OPT_EXPANDED)) {
      mu_layout_row(&mu_ctx, 5, (int[]){ 100, 100, 100, 100, 100 }, 18);
      
      // OSC gains
      for (int i = 0; i < 4; ++i) {
        char label[16];
        snprintf(label, sizeof(label), "OSC%d", i + 1);
        mu_begin_panel_ex(&mu_ctx, label, 0);
        mu_layout_row(&mu_ctx, 1, (int[]){ -1 }, 0);
        if (mu_slider(&mu_ctx, &s->mixer.osc_gain[i], 0.0f, 1.0f)) {
          char pname[16];
          snprintf(pname, sizeof(pname), "osc%d", i + 1);
          mixer_set_param(&s->mixer, pname, s->mixer.osc_gain[i]);
        }
        mu_end_panel(&mu_ctx);
      }
      
      // Master gain
      mu_begin_panel_ex(&mu_ctx, "Master", 0);
      mu_layout_row(&mu_ctx, 1, (int[]){ -1 }, 0);
      if (mu_slider(&mu_ctx, &s->mixer.master, 0.0f, 2.0f)) {
        mixer_set_param(&s->mixer, "master", s->mixer.master);
      }
      mu_end_panel(&mu_ctx);
    // }
    
    // Compressor section  
    // if (mu_header_ex(&mu_ctx, "Compressor", MU_OPT_EXPANDED)) {
      mu_layout_row(&mu_ctx, 2, (int[]){ 80, 80 }, 18);
      if (mu_checkbox(&mu_ctx, "Enabled", &s->mixer.comp_enabled)) {
        mixer_set_param(&s->mixer, "comp.enabled", (float)s->mixer.comp_enabled);
      }
      mu_layout_row(&mu_ctx, 2, (int[]){ 60, 100 }, 18);
      
      mu_label(&mu_ctx, "Threshold:");
      if (mu_slider(&mu_ctx, &s->mixer.comp_threshold, -24.0f, 0.0f)) {
        mixer_set_param(&s->mixer, "comp.threshold", s->mixer.comp_threshold);
      }
      
      mu_label(&mu_ctx, "Ratio:");
      if (mu_slider(&mu_ctx, &s->mixer.comp_ratio, 1.0f, 10.0f)) {
        mixer_set_param(&s->mixer, "comp.ratio", s->mixer.comp_ratio);
      }
      
      mu_label(&mu_ctx, "Attack:");
      if (mu_slider(&mu_ctx, &s->mixer.comp_attack, 1.0f, 100.0f)) {
        mixer_set_param(&s->mixer, "comp.attack", s->mixer.comp_attack);
      }
      
      mu_label(&mu_ctx, "Release:");
      if (mu_slider(&mu_ctx, &s->mixer.comp_release, 10.0f, 1000.0f)) {
        mixer_set_param(&s->mixer, "comp.release", s->mixer.comp_release);
      }
      
      mu_label(&mu_ctx, "Makeup:");
      if (mu_slider(&mu_ctx, &s->mixer.comp_makeup_gain, 0.0f, 12.0f)) {
        mixer_set_param(&s->mixer, "comp.makeup", s->mixer.comp_makeup_gain);
      }
    // } // This '}' closes the Compressor header if block
    
    // Arpeggiator
    // if (mu_header_ex(&mu_ctx, "Arpeggiator", MU_OPT_EXPANDED)) {
      mu_layout_row(&mu_ctx, 2, (int[]){ 80, 80 }, 18);
      if (mu_checkbox(&mu_ctx, "Enabled", &s->arp.enabled)) {
        arpeggiator_set_param(&s->arp, "enabled", s->arp.enabled);
      }
      mu_label(&mu_ctx, "Mode:");
      const char* arp_mode_options[] = {"UP", "DOWN", "ORDER", "RANDOM"};
      int arp_mode_idx = (int)s->arp.mode;
      if (mu_combo_box(&mu_ctx, arp_mode_options, 4, &arp_mode_idx, "arp_mode_popup")) {
        arpeggiator_set_param(&s->arp, "mode", (float)arp_mode_idx);
      }
      s->arp.mode = (ArpMode)arp_mode_idx;
      mu_layout_row(&mu_ctx, 2, (int[]){ 80, 80 }, 18);
      mu_label(&mu_ctx, "Tempo:");
      if (mu_slider(&mu_ctx, &s->arp.tempo, 30.0f, 240.0f)) {
        arpeggiator_set_param(&s->arp, "tempo", s->arp.tempo);
      }
      mu_layout_row(&mu_ctx, 2, (int[]){ 80, 80 }, 18);
      if (mu_checkbox(&mu_ctx, "Polyphonic", &s->arp.polyphonic)) {
        arpeggiator_set_param(&s->arp, "polyphonic", (float)s->arp.polyphonic);
      }
    // } // This '}' closes the Arpeggiator header if block
  } // Closing brace for if (mu_begin_window_ex(...))
  mu_end_window(&mu_ctx);
  
  // End microui frame
  mu_end(&mu_ctx);

  // Clear screen
  SDL_SetRenderDrawColor(r, 8, 16, 48, 255);
  SDL_RenderClear(r);

  // Render microui 
  render_commands();

  // Draw keyboard and oscilloscope using SDL directly
  int keyboard_width = window_width;
  int keyboard_height = window_height * 0.075f; 
  int keyboard_x = 0;
  int osc_h_px = window_height * 0.2f;
  int osc_y_px = window_height - osc_h_px;
  int keyboard_y = osc_y_px - keyboard_height;
  
  draw_keyboard(r, s, keyboard_x, keyboard_y, keyboard_width, keyboard_height);
  
  int osc_w_px = window_width;
  int osc_x_px = 0;
  oscilloscope_draw(r, s, osc_x_px, osc_y_px, osc_w_px, osc_h_px, gui_font);

  SDL_RenderPresent(r);
} // Closing brace for gui_draw function
