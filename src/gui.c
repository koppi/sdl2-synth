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

    float margin_x = window_width * 0.04f;
    float osc_area_w = window_width * 0.62f;
    float osc_area_h = window_height * 0.40f;
    float osc_w = osc_area_w / 4.0f;
    float osc_knob_y = margin_x * 1.5f;
    float osc_slider_y = osc_area_h * 0.45f;
    float osc_phase_y = osc_area_h * 0.75f;

    for (int i = 0; i < 4; ++i) {
      int base_x = margin_x + i * osc_w;
      int base_y = margin_x * 2;
      int pitch_x = base_x + osc_w * 0.18f;
      int detune_x = base_x + osc_w * 0.55f;
      int phase_x = base_x + osc_w * 0.43f;
      int pitch_r = osc_w * 0.18f;
      int detune_r = osc_w * 0.14f;
      int phase_r = osc_w * 0.11f;
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
    }

    float fx_area_y = window_height * 0.55f;
    float fx_knob_r = window_width * 0.027f;
    int fx_knob_y = fx_area_y;
    int fx_gap = window_width * 0.09f;
    int fx_xs[6] = {
        margin_x + fx_gap * 0,      margin_x + fx_gap * 1,
        margin_x + fx_gap * 2 + 20, margin_x + fx_gap * 3 + 20,
        margin_x + fx_gap * 4 + 60, margin_x + fx_gap * 5 + 60,
    };
    for (int i = 0; i < 6; ++i) {
      if (knob_hit(fx_xs[i], fx_knob_y, fx_knob_r, mx, my)) {
        hovered_control = 16 + i;
        break;
      }
    }

    float mixer_x = window_width * 0.68f;
    for (int i = 0; i < 4; ++i) {
      if (slider_hit(mixer_x, 100 + i * 50, 100, 20, mx, my)) {
        hovered_control = 22 + i;
        break;
      }
    }
    if (slider_hit(mixer_x, 350, 100, 30, mx, my))
      hovered_control = 26;

    int arp_knob_x = window_width - margin_x - 60;
    int arp_knob_y = 100;
    if (knob_hit(arp_knob_x, arp_knob_y, 30, mx, my))
      hovered_control = 27;
    if (knob_hit(arp_knob_x, arp_knob_y + 60, 10, mx, my))
      hovered_control = 28;
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
      } else if (sc == 27) {
        float v = gui->synth->arp.tempo - dy * 0.5f;
        if (v < 60.0f)
          v = 60.0f;
        if (v > 240.0f)
          v = 240.0f;
        arpeggiator_set_param(&gui->synth->arp, "tempo", v);
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
    } else if (sc == 27) {
      float v = gui->synth->arp.tempo + dy * 0.5f;
      if (v < 60.0f)
        v = 60.0f;
      if (v > 240.0f)
        v = 240.0f;
      arpeggiator_set_param(&gui->synth->arp, "tempo", v);
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
    draw_text(r, label, cx - text_w / 2, cy + radius + 6, GUI_TEXT_COLOR);
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

  float margin_x = window_width * 0.04f;
  float osc_area_w = window_width * 0.62f;
  float osc_area_h = window_height * 0.40f;
  float osc_w = osc_area_w / 4.0f;
  float osc_knob_y = margin_x * 1.5f;
  float osc_slider_y = osc_area_h * 0.45f;
  float osc_phase_y = osc_area_h * 0.75f;

  for (int i = 0; i < 4; ++i) {
    int base_x = margin_x + i * osc_w;
    int base_y = margin_x * 2;
    int pitch_x = base_x + osc_w * 0.18f;
    int detune_x = base_x + osc_w * 0.55f;
    int phase_x = base_x + osc_w * 0.43f;
    int pitch_r = osc_w * 0.18f;
    int detune_r = osc_w * 0.14f;
    int phase_r = osc_w * 0.11f;
    int cbase = i * 4;
    draw_knob(r, pitch_x, base_y + osc_knob_y, pitch_r, s->osc[i].pitch, -24,
              24, "Pitch",
              gui->selected_control == cbase + 0 ||
                  hovered_control == cbase + 0,
              pulse);
    draw_knob(r, detune_x, base_y + osc_knob_y, detune_r, s->osc[i].detune, -1,
              1, "Detune",
              gui->selected_control == cbase + 1 ||
                  hovered_control == cbase + 1,
              pulse);
    draw_slider(
        r, base_x, base_y + osc_slider_y, osc_w * 0.85f, 20, s->osc[i].gain, 0,
        1, gui->selected_control == cbase + 2 || hovered_control == cbase + 2,
        pulse, "Gain");
    draw_knob(r, phase_x, base_y + osc_phase_y, phase_r, s->osc[i].phase, 0, 1,
              "Phase",
              gui->selected_control == cbase + 3 ||
                  hovered_control == cbase + 3,
              pulse);
    int wf = (int)s->osc[i].waveform;
    for (int w = 0; w < 4; ++w)
      draw_led(r, base_x + 20 + w * 20, base_y + 55, wf == w);
  }
  float fx_area_y = window_height * 0.55f;
  float fx_knob_r = window_width * 0.027f;
  int fx_knob_y = fx_area_y;
  int fx_gap = window_width * 0.09f;
  int fx_xs[6] = {
      margin_x + fx_gap * 0,      margin_x + fx_gap * 1,
      margin_x + fx_gap * 2 + 20, margin_x + fx_gap * 3 + 20,
      margin_x + fx_gap * 4 + 60, margin_x + fx_gap * 5 + 60,
  };
  draw_knob(r, fx_xs[0], fx_knob_y, fx_knob_r, s->fx.flanger_depth, 0, 1,
            "FlangerDepth",
            gui->selected_control == 16 || hovered_control == 16, pulse);
  draw_knob(r, fx_xs[1], fx_knob_y, fx_knob_r, s->fx.flanger_rate, 0, 5,
            "FlangerRate", gui->selected_control == 17 || hovered_control == 17,
            pulse);
  draw_knob(r, fx_xs[2], fx_knob_y, fx_knob_r, s->fx.delay_time, 0, 1,
            "DelayTime", gui->selected_control == 18 || hovered_control == 18,
            pulse);
  draw_knob(r, fx_xs[3], fx_knob_y, fx_knob_r, s->fx.delay_feedback, 0, 1,
            "DelayFB", gui->selected_control == 19 || hovered_control == 19,
            pulse);
  draw_knob(r, fx_xs[4], fx_knob_y, fx_knob_r, s->fx.reverb_size, 0, 1,
            "ReverbSize", gui->selected_control == 20 || hovered_control == 20,
            pulse);
  draw_knob(r, fx_xs[5], fx_knob_y, fx_knob_r, s->fx.reverb_mix, 0, 1,
            "ReverbMix", gui->selected_control == 21 || hovered_control == 21,
            pulse);

  float mixer_x = window_width * 0.68f;
  for (int i = 0; i < 4; ++i)
    draw_slider(r, mixer_x, 100 + i * 50, 100, 20, s->mixer.osc_gain[i], 0, 1,
                gui->selected_control == 22 + i || hovered_control == 22 + i,
                pulse, "Mix");
  draw_slider(r, mixer_x, 350, 100, 30, s->mixer.master, 0, 2,
              gui->selected_control == 26 || hovered_control == 26, pulse,
              "Master");

  int arp_knob_x = window_width - margin_x - 60;
  int arp_knob_y = 100;
  draw_knob(r, arp_knob_x, arp_knob_y, 30, s->arp.tempo, 60, 240, "ArpTempo",
            gui->selected_control == 27 || hovered_control == 27, pulse);
  draw_led(r, arp_knob_x, arp_knob_y + 60, s->arp.enabled);

  int osc_w_px = window_width;
  int osc_h_px = window_height * 0.2f;
  int osc_x_px = 0;
  int osc_y_px = window_height - osc_h_px;
  oscilloscope_draw(r, s, osc_x_px, osc_y_px, osc_w_px, osc_h_px, gui_font);
}
