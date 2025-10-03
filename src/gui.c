#include "gui.h"
#include "oscilloscope.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int hovered_control = -1;
static TTF_Font *gui_font = NULL;

// Yellow color for text
static const SDL_Color GUI_TEXT_COLOR = {255, 220, 40, 255};

static int window_width = 1280, window_height = 720;

static void gui_get_window_size(SDL_Renderer *r) {
  (void)r; // Silence unused parameter warning
  SDL_Window *w =
      SDL_GetWindowFromID(1); // fallback: 1 is usually the first window
  if (w) {
    SDL_GetWindowSize(w, &window_width, &window_height);
  }
}

static void draw_text(SDL_Renderer *renderer, const char *text, int x, int y,
                      SDL_Color color) {
  if (!gui_font)
    return;
  SDL_Surface *surf = TTF_RenderText_Blended(gui_font, text, color);
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

static int knob_hit(int cx, int cy, int r, int mx, int my) {
  int dx = mx - cx, dy = my - cy;
  return (dx * dx + dy * dy <= r * r);
}
static int slider_hit(int x, int y, int w, int h, int mx, int my) {
  return (mx >= x && mx <= x + w && my >= y && my <= y + h);
}

// Combo box hit detection
static int combo_hit(int x, int y, int w, int h, int mx, int my) {
  return (mx >= x && mx <= x + w && my >= y && my <= y + h);
}

// Get waveform name
static const char* get_waveform_name(OscWaveform wf) {
  switch (wf) {
    case OSC_SINE: return "SINE";
    case OSC_SAW: return "SAW";
    case OSC_SQUARE: return "SQR";
    case OSC_TRI: return "TRI";
    default: return "???";
  }
}

typedef struct {
  float highlight_phase;
  int last_selected;
  Uint32 last_update_ticks;
} GuiAnimState;
static GuiAnimState anim_state = {0.0f, -1, 0};

void gui_init(Gui *gui, SDL_Renderer *renderer, Synth *synth) {
  gui->renderer = renderer;
  gui->synth = synth;
  gui->selected_control = -1;
  anim_state.highlight_phase = 0.0f;
  anim_state.last_selected = -1;
  anim_state.last_update_ticks = SDL_GetTicks();

  if (TTF_WasInit() == 0)
    TTF_Init();
  gui_font =
      TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
  if (!gui_font) {
    gui_font = TTF_OpenFont("DejaVuSans.ttf", 14);
  }
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

void gui_handle_event(Gui *gui, const SDL_Event *e) {
  static int mouse_drag_control = -1;
  // static int last_mouse_x = 0, last_mouse_y = 0; // last_mouse_x unused
  static int last_mouse_y = 0;

  if (e->type == SDL_WINDOWEVENT) {
    if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
      window_width = e->window.data1;
      window_height = e->window.data2;
    }
  }

  if (e->type == SDL_MOUSEMOTION) {
    int mx = e->motion.x, my = e->motion.y;
    // last_mouse_x = mx;
    last_mouse_y = my;
    hovered_control = -1;

    // Updated coordinates for top-left oscillator positioning
    float margin_x = window_width * 0.02f;
    float margin_y = window_height * 0.02f;
    float osc_area_w = window_width * 0.58f;
    float osc_area_h = window_height * 0.35f;
    float osc_w = osc_area_w / 4.0f;
    float osc_knob_y = margin_y + 30;
    float osc_slider_y = osc_area_h * 0.45f + margin_y;
    float osc_phase_y = osc_area_h * 0.75f + margin_y;

    for (int i = 0; i < 4; ++i) {
      int base_x = margin_x + i * osc_w;
      int base_y = margin_y; // Position at top
      int pitch_x = base_x + osc_w * 0.18f;
      int detune_x = base_x + osc_w * 0.55f;
      int phase_x = base_x + osc_w * 0.43f;
      int pitch_r = osc_w * 0.09f;   // Reduced from 0.18f to 0.09f (half size)
      int detune_r = osc_w * 0.09f;  // Standardized to same size as pitch
      int phase_r = osc_w * 0.09f;   // Standardized to same size as pitch
      if (knob_hit(pitch_x, base_y + osc_knob_y, pitch_r, mx, my)) {
        hovered_control = i * 4 + 0;
        break;
      }
      if (knob_hit(detune_x, base_y + osc_knob_y, detune_r, mx, my)) {
        hovered_control = i * 4 + 1;
        break;
      }
      if (slider_hit(base_x, base_y + osc_slider_y, osc_w * 0.85f, 20, mx,
                     my)) {
        hovered_control = i * 4 + 2;
        break;
      }
      if (knob_hit(phase_x, base_y + osc_phase_y, phase_r, mx, my)) {
        hovered_control = i * 4 + 3;
        break;
      }
      
      // Waveform combo box hit detection (controls 34-37)
      int combo_x = base_x + osc_w * 0.05f;
      int combo_y = base_y + osc_area_h * 0.85f;
      int combo_w = osc_w * 0.35f;
      int combo_h = 20;
      if (combo_hit(combo_x, combo_y, combo_w, combo_h, mx, my)) {
        hovered_control = 34 + i;  // Controls 34-37 for waveform selection
        break;
      }
    }

  // Define standard knob size for consistency throughout GUI
  float standard_knob_radius = window_width * 0.0125f;

  // Updated effects coordinates for below oscillators (must match drawing section)
  float fx_area_y = margin_y + osc_area_h + 40;
  int fx_knob_y = fx_area_y;
  int fx_gap = window_width * 0.085f;
  int fx_xs[6] = {
      margin_x + fx_gap * 0,      margin_x + fx_gap * 1,
      margin_x + fx_gap * 2 + 15, margin_x + fx_gap * 3 + 15,
      margin_x + fx_gap * 4 + 45, margin_x + fx_gap * 5 + 45,
  };
  for (int i = 0; i < 6; ++i) {
    if (knob_hit(fx_xs[i], fx_knob_y, standard_knob_radius, mx, my)) {
      hovered_control = 16 + i;
      break;
    }
  }

    // Updated mixer coordinates for horizontal layout below effects
    float mixer_y = fx_area_y + 80;  // Position below effects group
    float mixer_x = margin_x;        // Start at left margin like effects
    for (int i = 0; i < 4; ++i) {
      // Horizontal layout with same spacing as effects
      int slider_x = mixer_x + (fx_gap * i);
      if (slider_hit(slider_x, mixer_y + 20, 80, 20, mx, my)) {  // Horizontal sliders
        hovered_control = 22 + i;
        break;
      }
    }
    // Master slider positioned after the 4 OSC sliders
    int master_x = mixer_x + (fx_gap * 4) + 20;
    if (slider_hit(master_x, mixer_y + 20, 80, 20, mx, my))
      hovered_control = 26;

    // Remove arp tempo knob - keep only arp LED
    int arp_knob_x = window_width - margin_x - 60;
    int arp_knob_y = 100;
    if (knob_hit(arp_knob_x, arp_knob_y + 60, 10, mx, my))
      hovered_control = 28;

    // Updated compressor coordinates for vertical layout in top right
    float comp_area_x = window_width * 0.65f;  // Right side positioning
    int comp_knobs_x = comp_area_x;            // Fixed X position for vertical layout
    int comp_start_y = margin_y + 30;          // Start Y position
    int comp_gap_y = 60;                       // Vertical gap between knobs
    int comp_xs[5] = {
        comp_knobs_x, comp_knobs_x, comp_knobs_x, comp_knobs_x, comp_knobs_x  // All same X
    };
    int comp_ys[5] = {
        comp_start_y + comp_gap_y * 0,  // Threshold
        comp_start_y + comp_gap_y * 1,  // Ratio  
        comp_start_y + comp_gap_y * 2,  // Attack
        comp_start_y + comp_gap_y * 3,  // Release
        comp_start_y + comp_gap_y * 4,  // Makeup
    };
    for (int i = 0; i < 5; ++i) {
      if (knob_hit(comp_xs[i], comp_ys[i], standard_knob_radius, mx, my)) {
        hovered_control = 29 + i;  // Controls 29-33 for compressor
        break;
      }
    }
  }

  if (e->type == SDL_MOUSEBUTTONDOWN) {
    int mx = e->button.x, my = e->button.y;
    // last_mouse_x = mx;
    last_mouse_y = my;
    if (hovered_control != -1) {
      gui->selected_control = hovered_control;
      mouse_drag_control = hovered_control;
      if (hovered_control == 28) {
        arpeggiator_set_param(&gui->synth->arp, "enabled",
                              !gui->synth->arp.enabled);
      }
      if (hovered_control >= 34 && hovered_control <= 37) {
        int osc_idx = hovered_control - 34;
        OscWaveform current = gui->synth->osc[osc_idx].waveform;
        OscWaveform next = (OscWaveform)((current + 1) % 4);
        osc_set_param(&gui->synth->osc[osc_idx], "waveform", (float)next);
      }
    } else {
      gui->selected_control = -1;
      mouse_drag_control = -1;
    }
  } else if (e->type == SDL_MOUSEBUTTONUP) {
    mouse_drag_control = -1;
  } else if (e->type == SDL_MOUSEMOTION) {
    if (mouse_drag_control != -1 && (e->motion.state & SDL_BUTTON_LMASK)) {
      int dy = e->motion.y - last_mouse_y;
      int sc = mouse_drag_control;
      if (sc >= 0 && sc < 16) {
        int osc = sc / 4;
        int param = sc % 4;
        Oscillator *o = &gui->synth->osc[osc];
        if (param == 0) {
          float new_pitch = o->pitch - dy * 0.1f;
          if (new_pitch < -24.0f)
            new_pitch = -24.0f;
          if (new_pitch > 24.0f)
            new_pitch = 24.0f;
          osc_set_param(o, "pitch", new_pitch);
        } else if (param == 1) {
          float new_det = o->detune - dy * 0.01f;
          if (new_det < -1.0f)
            new_det = -1.0f;
          if (new_det > 1.0f)
            new_det = 1.0f;
          osc_set_param(o, "detune", new_det);
        } else if (param == 2) {
          float new_gain = o->gain - dy * 0.005f;
          if (new_gain < 0.0f)
            new_gain = 0.0f;
          if (new_gain > 1.0f)
            new_gain = 1.0f;
          osc_set_param(o, "gain", new_gain);
        } else if (param == 3) {
          float new_phase = o->phase - dy * 0.01f;
          if (new_phase < 0.0f)
            new_phase = 0.0f;
          if (new_phase > 1.0f)
            new_phase = 1.0f;
          osc_set_param(o, "phase", new_phase);
        }
      } else if (sc == 16) {
        float v = gui->synth->fx.flanger_depth - dy * 0.01f;
        if (v < 0.0f)
          v = 0.0f;
        if (v > 1.0f)
          v = 1.0f;
        fx_set_param(&gui->synth->fx, "flanger.depth", v);
      } else if (sc == 17) {
        float v = gui->synth->fx.flanger_rate - dy * 0.01f;
        if (v < 0.0f)
          v = 0.0f;
        if (v > 5.0f)
          v = 5.0f;
        fx_set_param(&gui->synth->fx, "flanger.rate", v);
      } else if (sc == 18) {
        float v = gui->synth->fx.delay_time - dy * 0.005f;
        if (v < 0.0f)
          v = 0.0f;
        if (v > 1.0f)
          v = 1.0f;
        fx_set_param(&gui->synth->fx, "delay.time", v);
      } else if (sc == 19) {
        float v = gui->synth->fx.delay_feedback - dy * 0.005f;
        if (v < 0.0f)
          v = 0.0f;
        if (v > 1.0f)
          v = 1.0f;
        fx_set_param(&gui->synth->fx, "delay.feedback", v);
      } else if (sc == 20) {
        float v = gui->synth->fx.reverb_size - dy * 0.01f;
        if (v < 0.0f)
          v = 0.0f;
        if (v > 1.0f)
          v = 1.0f;
        fx_set_param(&gui->synth->fx, "reverb.size", v);
      } else if (sc == 21) {
        float v = gui->synth->fx.reverb_mix - dy * 0.01f;
        if (v < 0.0f)
          v = 0.0f;
        if (v > 1.0f)
          v = 1.0f;
        fx_set_param(&gui->synth->fx, "reverb.mix", v);
      } else if (sc >= 22 && sc < 26) {
        int mi = sc - 22;
        float v = gui->synth->mixer.osc_gain[mi] - dy * 0.01f;
        if (v < 0.0f)
          v = 0.0f;
        if (v > 1.0f)
          v = 1.0f;
        char pname[16];
        snprintf(pname, sizeof(pname), "osc%d", mi + 1);
        mixer_set_param(&gui->synth->mixer, pname, v);
      } else if (sc == 26) {
        float v = gui->synth->mixer.master - dy * 0.01f;
        if (v < 0.0f)
          v = 0.0f;
        if (v > 2.0f)
          v = 2.0f;
        mixer_set_param(&gui->synth->mixer, "master", v);
      } else if (sc >= 29 && sc <= 33) {
        // Bus compressor controls (29-33)
        int comp_param = sc - 29;
        if (comp_param == 0) {  // Threshold
          float v = gui->synth->mixer.comp_threshold - dy * 0.5f;
          if (v < -24.0f) v = -24.0f;
          if (v > 0.0f) v = 0.0f;
          mixer_set_param(&gui->synth->mixer, "comp.threshold", v);
        } else if (comp_param == 1) {  // Ratio
          float v = gui->synth->mixer.comp_ratio - dy * 0.1f;
          if (v < 1.0f) v = 1.0f;
          if (v > 10.0f) v = 10.0f;
          mixer_set_param(&gui->synth->mixer, "comp.ratio", v);
        } else if (comp_param == 2) {  // Attack
          float v = gui->synth->mixer.comp_attack - dy * 0.5f;
          if (v < 1.0f) v = 1.0f;
          if (v > 100.0f) v = 100.0f;
          mixer_set_param(&gui->synth->mixer, "comp.attack", v);
        } else if (comp_param == 3) {  // Release
          float v = gui->synth->mixer.comp_release - dy * 2.0f;
          if (v < 10.0f) v = 10.0f;
          if (v > 1000.0f) v = 1000.0f;
          mixer_set_param(&gui->synth->mixer, "comp.release", v);
        } else if (comp_param == 4) {  // Makeup gain
          float v = gui->synth->mixer.comp_makeup_gain - dy * 0.1f;
          if (v < 0.0f) v = 0.0f;
          if (v > 12.0f) v = 12.0f;
          mixer_set_param(&gui->synth->mixer, "comp.makeup", v);
        }
      }
      last_mouse_y = e->motion.y;
    }
  } else if (e->type == SDL_MOUSEWHEEL) {
    int sc = hovered_control;
    int dy = e->wheel.y;
    if (sc >= 0 && sc < 16) {
      int osc = sc / 4;
      int param = sc % 4;
      Oscillator *o = &gui->synth->osc[osc];
      if (param == 0) {
        float new_pitch = o->pitch + dy * 1.0f;
        if (new_pitch < -24.0f)
          new_pitch = -24.0f;
        if (new_pitch > 24.0f)
          new_pitch = 24.0f;
        osc_set_param(o, "pitch", new_pitch);
      } else if (param == 1) {
        float new_det = o->detune + dy * 0.01f;
        if (new_det < -1.0f)
          new_det = -1.0f;
        if (new_det > 1.0f)
          new_det = 1.0f;
        osc_set_param(o, "detune", new_det);
      } else if (param == 2) {
        float new_gain = o->gain + dy * 0.01f;
        if (new_gain < 0.0f)
          new_gain = 0.0f;
        if (new_gain > 1.0f)
          new_gain = 1.0f;
        osc_set_param(o, "gain", new_gain);
      } else if (param == 3) {
        float new_phase = o->phase + dy * 0.01f;
        if (new_phase < 0.0f)
          new_phase = 0.0f;
        if (new_phase > 1.0f)
          new_phase = 1.0f;
        osc_set_param(o, "phase", new_phase);
      }
    } else if (sc == 16) {
      float v = gui->synth->fx.flanger_depth + dy * 0.01f;
      if (v < 0.0f)
        v = 0.0f;
      if (v > 1.0f)
        v = 1.0f;
      fx_set_param(&gui->synth->fx, "flanger.depth", v);
    } else if (sc == 17) {
      float v = gui->synth->fx.flanger_rate + dy * 0.01f;
      if (v < 0.0f)
        v = 0.0f;
      if (v > 5.0f)
        v = 5.0f;
      fx_set_param(&gui->synth->fx, "flanger.rate", v);
    } else if (sc == 18) {
      float v = gui->synth->fx.delay_time + dy * 0.005f;
      if (v < 0.0f)
        v = 0.0f;
      if (v > 1.0f)
        v = 1.0f;
      fx_set_param(&gui->synth->fx, "delay.time", v);
    } else if (sc == 19) {
      float v = gui->synth->fx.delay_feedback + dy * 0.005f;
      if (v < 0.0f)
        v = 0.0f;
      if (v > 1.0f)
        v = 1.0f;
      fx_set_param(&gui->synth->fx, "delay.feedback", v);
    } else if (sc == 20) {
      float v = gui->synth->fx.reverb_size + dy * 0.01f;
      if (v < 0.0f)
        v = 0.0f;
      if (v > 1.0f)
        v = 1.0f;
      fx_set_param(&gui->synth->fx, "reverb.size", v);
    } else if (sc == 21) {
      float v = gui->synth->fx.reverb_mix + dy * 0.01f;
      if (v < 0.0f)
        v = 0.0f;
      if (v > 1.0f)
        v = 1.0f;
      fx_set_param(&gui->synth->fx, "reverb.mix", v);
    } else if (sc >= 22 && sc < 26) {
      int mi = sc - 22;
      float v = gui->synth->mixer.osc_gain[mi] + dy * 0.01f;
      if (v < 0.0f)
        v = 0.0f;
      if (v > 1.0f)
        v = 1.0f;
      char pname[16];
      snprintf(pname, sizeof(pname), "osc%d", mi + 1);
      mixer_set_param(&gui->synth->mixer, pname, v);
    } else if (sc == 26) {
      float v = gui->synth->mixer.master + dy * 0.01f;
      if (v < 0.0f)
        v = 0.0f;
      if (v > 2.0f)
        v = 2.0f;
      mixer_set_param(&gui->synth->mixer, "master", v);
    } else if (sc >= 29 && sc <= 33) {
      // Bus compressor controls (mouse wheel)
      int comp_param = sc - 29;
      if (comp_param == 0) {  // Threshold
        float v = gui->synth->mixer.comp_threshold + dy * 0.5f;
        if (v < -24.0f) v = -24.0f;
        if (v > 0.0f) v = 0.0f;
        mixer_set_param(&gui->synth->mixer, "comp.threshold", v);
      } else if (comp_param == 1) {  // Ratio
        float v = gui->synth->mixer.comp_ratio + dy * 0.1f;
        if (v < 1.0f) v = 1.0f;
        if (v > 10.0f) v = 10.0f;
        mixer_set_param(&gui->synth->mixer, "comp.ratio", v);
      } else if (comp_param == 2) {  // Attack
        float v = gui->synth->mixer.comp_attack + dy * 0.5f;
        if (v < 1.0f) v = 1.0f;
        if (v > 100.0f) v = 100.0f;
        mixer_set_param(&gui->synth->mixer, "comp.attack", v);
      } else if (comp_param == 3) {  // Release
        float v = gui->synth->mixer.comp_release + dy * 2.0f;
        if (v < 10.0f) v = 10.0f;
        if (v > 1000.0f) v = 1000.0f;
        mixer_set_param(&gui->synth->mixer, "comp.release", v);
      } else if (comp_param == 4) {  // Makeup gain
        float v = gui->synth->mixer.comp_makeup_gain + dy * 0.1f;
        if (v < 0.0f) v = 0.0f;
        if (v > 12.0f) v = 12.0f;
        mixer_set_param(&gui->synth->mixer, "comp.makeup", v);
      }
    } else if (sc >= 34 && sc <= 37) {
      // Waveform selection mouse wheel
      int osc_idx = sc - 34;
      OscWaveform current = gui->synth->osc[osc_idx].waveform;
      OscWaveform next;
      if (dy > 0) {
        next = (OscWaveform)((current + 1) % 4);  // Next waveform
      } else {
        next = (OscWaveform)((current + 3) % 4);  // Previous waveform (wrap around)
      }
      osc_set_param(&gui->synth->osc[osc_idx], "waveform", (float)next);
    }
  }
}

void gui_update(Gui *gui) {
  Uint32 now = SDL_GetTicks();
  float dt = (now - anim_state.last_update_ticks) / 1000.0f;
  anim_state.last_update_ticks = now;
  if (gui->selected_control != -1) {
    anim_state.highlight_phase += dt * 1.5f;
    if (anim_state.highlight_phase > 1.0f)
      anim_state.highlight_phase -= 1.0f;
    anim_state.last_selected = gui->selected_control;
  } else {
    anim_state.highlight_phase *= 0.92f;
  }
}

static void draw_knob_border(SDL_Renderer *r, int cx, int cy, int radius,
                             float anim_phase) {
  Uint8 cr = (Uint8)(255.0f * (0.7f + 0.3f * sinf(anim_phase * 2.0f * M_PI)));
  Uint8 cg =
      (Uint8)(200.0f + 55.0f * (0.7f + 0.3f * sinf(anim_phase * 2.0f * M_PI)));
  Uint8 cb = 50;
  SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
  for (int d = radius + 2; d <= radius + 4; ++d) {
    for (int angle = 0; angle < 360; ++angle) {
      float theta = angle * (float)M_PI / 180.0f;
      int x = (int)(cx + d * cosf(theta));
      int y = (int)(cy + d * sinf(theta));
      SDL_RenderDrawPoint(r, x, y);
    }
  }
}

static void draw_slider_border(SDL_Renderer *r, int x, int y, int w, int h,
                               float anim_phase) {
  Uint8 cr = (Uint8)(255.0f * (0.7f + 0.3f * sinf(anim_phase * 2.0f * M_PI)));
  Uint8 cg =
      (Uint8)(200.0f + 55.0f * (0.7f + 0.3f * sinf(anim_phase * 2.0f * M_PI)));
  Uint8 cb = 50;
  SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
  SDL_Rect rect = {x - 2, y - 2, w + 4, h + 4};
  SDL_RenderDrawRect(r, &rect);
  rect.x -= 1;
  rect.y -= 1;
  rect.w += 2;
  rect.h += 2;
  SDL_RenderDrawRect(r, &rect);
}

static void draw_knob(SDL_Renderer *r, int cx, int cy, int radius, float value,
                      float min, float max, const char *label, int highlight,
                      float anim_phase) {
  SDL_SetRenderDrawColor(r, highlight ? 240 : 200, highlight ? 220 : 200, 80,
                         255);
  for (int dy = -radius; dy <= radius; ++dy)
    for (int dx = -radius; dx <= radius; ++dx)
      if (dx * dx + dy * dy <= radius * radius)
        SDL_RenderDrawPoint(r, cx + dx, cy + dy);

  float angle =
      (value - min) / (max - min) * (5.0f * M_PI / 4.0f) + (7.0f * M_PI / 8.0f);
  int mx = (int)(cx + (radius - 5) * cosf(angle));
  int my = (int)(cy + (radius - 5) * sinf(angle));
  SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
  SDL_RenderDrawLine(r, cx, cy, mx, my);

  if (highlight)
    draw_knob_border(r, cx, cy, radius, anim_phase);

  if (label && label[0] && gui_font) {
    int text_w = 0, text_h = 0;
    TTF_SizeText(gui_font, label, &text_w, &text_h);
  }

  char valstr[32];
  snprintf(valstr, sizeof(valstr), "%.2f", value);
  int val_w = 0, val_h = 0;
  TTF_SizeText(gui_font, valstr, &val_w, &val_h);
  draw_text(r, valstr, cx - val_w / 2, cy - radius - val_h - 2, GUI_TEXT_COLOR);
}

static void draw_slider(SDL_Renderer *r, int x, int y, int w, int h,
                        float value, float min, float max, int highlight,
                        float anim_phase, const char *label) {
  SDL_Rect rail = {x, y + h / 2 - 2, w, 4};
  SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
  SDL_RenderFillRect(r, &rail);

  int tx = x + (int)((value - min) / (max - min) * (w - 10));
  SDL_Rect thumb = {tx, y, 10, h};
  SDL_SetRenderDrawColor(r, highlight ? 255 : 200, highlight ? 210 : 200, 50,
                         255);
  SDL_RenderFillRect(r, &thumb);

  if (highlight)
    draw_slider_border(r, tx, y, 10, h, anim_phase);

  char valstr[32];
  snprintf(valstr, sizeof(valstr), "%.2f", value);
  int val_w = 0, val_h = 0;
  TTF_SizeText(gui_font, valstr, &val_w, &val_h);
  draw_text(r, valstr, tx + 5 - val_w / 2, y - 18, GUI_TEXT_COLOR);

  if (label && label[0] && gui_font) {
    int text_w = 0, text_h = 0;
    TTF_SizeText(gui_font, label, &text_w, &text_h);
    draw_text(r, label, x + (w - text_w) / 2, y + h + 2, GUI_TEXT_COLOR);
  }
}

// Draw combo box for waveform selection
static void draw_combo(SDL_Renderer *r, int x, int y, int w, int h,
                       const char *text, int highlight, float anim_phase) {
  // Background
  SDL_Rect combo_rect = {x, y, w, h};
  SDL_SetRenderDrawColor(r, highlight ? 60 : 40, highlight ? 60 : 40, 
                         highlight ? 80 : 60, 255);
  SDL_RenderFillRect(r, &combo_rect);
  
  // Border
  SDL_SetRenderDrawColor(r, highlight ? 120 : 100, highlight ? 120 : 100, 
                         highlight ? 140 : 120, 255);
  SDL_RenderDrawRect(r, &combo_rect);
  
  // Highlight border animation
  if (highlight) {
    Uint8 cr = (Uint8)(255.0f * (0.7f + 0.3f * sinf(anim_phase * 2.0f * M_PI)));
    Uint8 cg = (Uint8)(200.0f + 55.0f * (0.7f + 0.3f * sinf(anim_phase * 2.0f * M_PI)));
    Uint8 cb = 50;
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_Rect highlight_rect = {x - 1, y - 1, w + 2, h + 2};
    SDL_RenderDrawRect(r, &highlight_rect);
  }
  
  // Text
  if (text && gui_font) {
    int text_w = 0, text_h = 0;
    TTF_SizeText(gui_font, text, &text_w, &text_h);
    draw_text(r, text, x + (w - text_w) / 2, y + (h - text_h) / 2, GUI_TEXT_COLOR);
  }
  
  // Dropdown arrow
  SDL_SetRenderDrawColor(r, 180, 180, 200, 255);
  int arrow_x = x + w - 12;
  int arrow_y = y + h / 2;
  // Simple down arrow (triangle)
  SDL_RenderDrawLine(r, arrow_x, arrow_y - 2, arrow_x + 4, arrow_y + 2);
  SDL_RenderDrawLine(r, arrow_x + 4, arrow_y + 2, arrow_x + 8, arrow_y - 2);
}

static void draw_led(SDL_Renderer *r, int x, int y, int on) {
  SDL_SetRenderDrawColor(r, on ? 0 : 40, on ? 255 : 40, 20, 255);
  for (int dy = -5; dy <= 5; ++dy)
    for (int dx = -5; dx <= 5; ++dx)
      if (dx * dx + dy * dy <= 16)
        SDL_RenderDrawPoint(r, x + dx, y + dy);
}

void gui_draw(Gui *gui) {
  SDL_Renderer *r = gui->renderer;
  Synth *s = gui->synth;
  float pulse = anim_state.highlight_phase;

  gui_get_window_size(r);

  SDL_SetRenderDrawColor(r, 8, 16, 48, 255);
  SDL_RenderClear(r);

  // Improved layout with oscillators at top left
  float margin_x = window_width * 0.02f;  // Small margin from left edge
  float margin_y = window_height * 0.02f; // Small margin from top edge
  float osc_area_w = window_width * 0.58f; // Width for 4 oscillators
  float osc_area_h = window_height * 0.35f; // Height for oscillator section
  float osc_w = osc_area_w / 4.0f;
  float osc_knob_y = margin_y + 30;  // Position from top
  float osc_slider_y = osc_area_h * 0.45f + margin_y;
  float osc_phase_y = osc_area_h * 0.75f + margin_y;

  for (int i = 0; i < 4; ++i) {
    int base_x = margin_x + i * osc_w;
    int base_y = margin_y; // Position at top of window
    
    // Draw oscillator border/grouping
    SDL_Rect osc_border = {
        base_x - 5,           // Left padding
        base_y - 5,           // Top padding  
        osc_w - 10,           // Width with padding
        osc_area_h + 10       // Height with padding
    };
    
    // Border background (subtle)
    SDL_SetRenderDrawColor(r, 25, 30, 45, 180);
    SDL_RenderFillRect(r, &osc_border);
    
    // Border outline
    SDL_SetRenderDrawColor(r, 80, 85, 100, 255);
    SDL_RenderDrawRect(r, &osc_border);
    
    // OSC label at top of border
    char osc_label[8];
    snprintf(osc_label, sizeof(osc_label), "OSC%d", i + 1);
    if (gui_font) {
      int label_w = 0, label_h = 0;
      TTF_SizeText(gui_font, osc_label, &label_w, &label_h);
      draw_text(r, osc_label, base_x + (osc_w - label_w) / 2 - 5, base_y - 3, GUI_TEXT_COLOR);
    }
    
    int pitch_x = base_x + osc_w * 0.18f;
    int detune_x = base_x + osc_w * 0.55f;
    int phase_x = base_x + osc_w * 0.43f;
    int standard_knob_r = osc_w * 0.09f;  // Standard knob radius for all oscillator knobs
    int cbase = i * 4;
    draw_knob(r, pitch_x, base_y + osc_knob_y, standard_knob_r, s->osc[i].pitch, -24,
              24, "Pitch",
              gui->selected_control == cbase + 0 ||
                  hovered_control == cbase + 0,
              pulse);
    draw_knob(r, detune_x, base_y + osc_knob_y, standard_knob_r, s->osc[i].detune, -1,
              1, "Detune",
              gui->selected_control == cbase + 1 ||
                  hovered_control == cbase + 1,
              pulse);
    draw_slider(
        r, base_x, base_y + osc_slider_y, osc_w * 0.85f, 20, s->osc[i].gain, 0,
        1, gui->selected_control == cbase + 2 || hovered_control == cbase + 2,
        pulse, "Gain");
    draw_knob(r, phase_x, base_y + osc_phase_y, standard_knob_r, s->osc[i].phase, 0, 1,
              "Phase",
              gui->selected_control == cbase + 3 ||
                  hovered_control == cbase + 3,
              pulse);
              
    // Waveform selection combo box
    int combo_x = base_x + osc_w * 0.05f;
    int combo_y = base_y + osc_area_h * 0.85f;
    int combo_w = osc_w * 0.35f;
    int combo_h = 20;
    const char* waveform_name = get_waveform_name(s->osc[i].waveform);
    draw_combo(r, combo_x, combo_y, combo_w, combo_h, waveform_name,
               gui->selected_control == 34 + i || hovered_control == 34 + i, pulse);
  }
  // Use standard knob size for all knobs throughout the interface
  float standard_knob_radius = window_width * 0.0125f;

  // Move effects section below oscillators
  float fx_area_y = margin_y + osc_area_h + 40;  // Position below oscillators
  int fx_knob_y = fx_area_y;
  int fx_gap = window_width * 0.085f;
  int fx_xs[6] = {
      margin_x + fx_gap * 0,      margin_x + fx_gap * 1,
      margin_x + fx_gap * 2 + 15, margin_x + fx_gap * 3 + 15,
      margin_x + fx_gap * 4 + 45, margin_x + fx_gap * 5 + 45,
  };
  
  // Draw effects border/grouping with proper left margin alignment
  SDL_Rect fx_border = {
      margin_x - 10,                    // Left margin aligned with oscillators
      fx_knob_y - 25,                  // Top (above knobs)
      fx_xs[5] + 40 - margin_x + 20,   // Width (covers all fx knobs with proper margins)
      80                               // Height (covers knobs + labels)
  };
  
  // Border background (subtle)
  SDL_SetRenderDrawColor(r, 25, 30, 45, 180);
  SDL_RenderFillRect(r, &fx_border);
  
  // Border outline
  SDL_SetRenderDrawColor(r, 80, 85, 100, 255);
  SDL_RenderDrawRect(r, &fx_border);
  
  // EFFECTS label at top of border with proper alignment
  if (gui_font) {
    int label_w = 0, label_h = 0;
    TTF_SizeText(gui_font, "EFFECTS", &label_w, &label_h);
    draw_text(r, "EFFECTS", margin_x, fx_knob_y - 20, GUI_TEXT_COLOR);  // Aligned with left margin
  }
  
  draw_knob(r, fx_xs[0], fx_knob_y, standard_knob_radius, s->fx.flanger_depth, 0, 1,
            "FlangerDepth",
            gui->selected_control == 16 || hovered_control == 16, pulse);
  draw_knob(r, fx_xs[1], fx_knob_y, standard_knob_radius, s->fx.flanger_rate, 0, 5,
            "FlangerRate", gui->selected_control == 17 || hovered_control == 17,
            pulse);
  draw_knob(r, fx_xs[2], fx_knob_y, standard_knob_radius, s->fx.delay_time, 0, 1,
            "DelayTime", gui->selected_control == 18 || hovered_control == 18,
            pulse);
  draw_knob(r, fx_xs[3], fx_knob_y, standard_knob_radius, s->fx.delay_feedback, 0, 1,
            "DelayFB", gui->selected_control == 19 || hovered_control == 19,
            pulse);
  draw_knob(r, fx_xs[4], fx_knob_y, standard_knob_radius, s->fx.reverb_size, 0, 1,
            "ReverbSize", gui->selected_control == 20 || hovered_control == 20,
            pulse);
  draw_knob(r, fx_xs[5], fx_knob_y, standard_knob_radius, s->fx.reverb_mix, 0, 1,
            "ReverbMix", gui->selected_control == 21 || hovered_control == 21,
            pulse);

  // Position mixer below effects in horizontal layout
  float mixer_y = fx_area_y + 80;  // Position below effects group
  float mixer_x = margin_x;        // Start at left margin like effects
  
  // Calculate horizontal positions for mixer sliders
  int mixer_positions[5];
  for (int i = 0; i < 4; ++i) {
    mixer_positions[i] = mixer_x + (fx_gap * i);  // Same spacing as effects
  }
  mixer_positions[4] = mixer_x + (fx_gap * 4) + 20;  // Master slider with gap
  
  // Draw mixer border/grouping for horizontal layout
  SDL_Rect mixer_border = {
      mixer_x - 10,                           // Left margin aligned with effects
      mixer_y - 5,                           // Top position  
      mixer_positions[4] + 90 - mixer_x + 10, // Width to cover all sliders
      50                                     // Height for horizontal layout
  };
  
  // Border background (subtle)
  SDL_SetRenderDrawColor(r, 25, 30, 45, 180);
  SDL_RenderFillRect(r, &mixer_border);
  
  // Border outline
  SDL_SetRenderDrawColor(r, 80, 85, 100, 255);
  SDL_RenderDrawRect(r, &mixer_border);
  
  // MIXER label at top of horizontal border
  if (gui_font) {
    int label_w = 0, label_h = 0;
    TTF_SizeText(gui_font, "MIXER", &label_w, &label_h);
    draw_text(r, "MIXER", mixer_x, mixer_y - 20, GUI_TEXT_COLOR);  // Aligned with left margin
  }
  
  // Draw horizontal mixer sliders
  for (int i = 0; i < 4; ++i) {
    char label[8];
    snprintf(label, sizeof(label), "OSC%d", i + 1);
    draw_slider(r, mixer_positions[i], mixer_y + 20, 80, 20, s->mixer.osc_gain[i], 0, 1,
                gui->selected_control == 22 + i || hovered_control == 22 + i,
                pulse, label);
  }
  
  // Master slider with larger size
  draw_slider(r, mixer_positions[4], mixer_y + 20, 80, 25, s->mixer.master, 0, 2,
              gui->selected_control == 26 || hovered_control == 26, pulse,
              "MASTER");

  // Arp LED only (tempo knob removed)
  int arp_knob_x = window_width - margin_x - 60;
  int arp_knob_y = 100;
  draw_led(r, arp_knob_x, arp_knob_y + 60, s->arp.enabled);

  // Position compressor vertically in top right area
  float comp_area_x = window_width * 0.65f;  // Right side positioning
  int comp_knobs_x = comp_area_x;            // Fixed X position for vertical layout
  int comp_start_y = margin_y + 30;          // Start Y position
  int comp_gap_y = 60;                       // Vertical gap between knobs
  int comp_xs[5] = {
      comp_knobs_x, comp_knobs_x, comp_knobs_x, comp_knobs_x, comp_knobs_x  // All same X
  };
  int comp_ys[5] = {
      comp_start_y + comp_gap_y * 0,  // Threshold
      comp_start_y + comp_gap_y * 1,  // Ratio  
      comp_start_y + comp_gap_y * 2,  // Attack
      comp_start_y + comp_gap_y * 3,  // Release
      comp_start_y + comp_gap_y * 4,  // Makeup
  };
  
  // Draw compressor border/grouping for vertical layout
  SDL_Rect comp_border = {
      comp_knobs_x - 30,                    // Left padding
      comp_start_y - 25,                   // Top (above first knob)  
      80,                                   // Width (compact for vertical)
      comp_ys[4] - comp_start_y + 60      // Height (covers all knobs)
  };
  
  // Border background (subtle)
  SDL_SetRenderDrawColor(r, 25, 30, 45, 180);
  SDL_RenderFillRect(r, &comp_border);
  
  // Border outline
  SDL_SetRenderDrawColor(r, 80, 85, 100, 255);
  SDL_RenderDrawRect(r, &comp_border);
  
  // COMP label at top of vertical border
  if (gui_font) {
    int label_w = 0, label_h = 0;
    TTF_SizeText(gui_font, "COMP", &label_w, &label_h);
    draw_text(r, "COMP", comp_knobs_x - 15, comp_start_y - 20, GUI_TEXT_COLOR);
  }
  
  // Draw vertical compressor knobs
  draw_knob(r, comp_xs[0], comp_ys[0], standard_knob_radius, s->mixer.comp_threshold, -24, 0,
            "Threshold", gui->selected_control == 29 || hovered_control == 29, pulse);
  draw_knob(r, comp_xs[1], comp_ys[1], standard_knob_radius, s->mixer.comp_ratio, 1, 10,
            "Ratio", gui->selected_control == 30 || hovered_control == 30, pulse);
  draw_knob(r, comp_xs[2], comp_ys[2], standard_knob_radius, s->mixer.comp_attack, 1, 100,
            "Attack", gui->selected_control == 31 || hovered_control == 31, pulse);
  draw_knob(r, comp_xs[3], comp_ys[3], standard_knob_radius, s->mixer.comp_release, 10, 1000,
            "Release", gui->selected_control == 32 || hovered_control == 32, pulse);
  draw_knob(r, comp_xs[4], comp_ys[4], standard_knob_radius, s->mixer.comp_makeup_gain, 0, 12,
            "Makeup", gui->selected_control == 33 || hovered_control == 33, pulse);

  int osc_w_px = window_width;
  int osc_h_px = window_height * 0.2f;
  int osc_x_px = 0;
  int osc_y_px = window_height - osc_h_px;
  oscilloscope_draw(r, s, osc_x_px, osc_y_px, osc_w_px, osc_h_px, gui_font);
}
