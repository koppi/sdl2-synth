#include "app.h"
#include <stdio.h>
#include <string.h>
#include "oscilloscope.h"

int app_init(App *app) {
  memset(app, 0, sizeof(App));
  
  // Initialize timing for 60 FPS
  app->target_frame_time = 1000 / 60; // ~16.67ms per frame
  app->last_frame_time = SDL_GetTicks();
  app->frame_count = 0;
  app->fps = 0.0f;
  
  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
    return 0;
  }

  if (TTF_Init() == -1) {
    return 0;
  }

  SDL_DisplayMode dm;
  if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
    SDL_Log("SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());
    // Fallback to default resolution if getting display mode fails
    dm.w = 1280;
    dm.h = 720;
  }

  app->window = SDL_CreateWindow("Modular Synth", SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, dm.w, dm.h,
                                 SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!app->window) {
    return 0;
  }

  app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED);
  if (!app->renderer) {
    return 0;
  }

  if (!synth_init(&app->synth, 48000, 1024, 64)) {
    return 0;
  }

  gui_init(&app->gui, app->renderer, &app->synth);
  app->font = gui_get_font();
  oscilloscope_init();

  SDL_AudioSpec want = {0}, got = {0};
  want.freq = 48000;
  want.format = AUDIO_F32SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = synth_audio_callback;
  want.userdata = &app->synth;
  app->audio = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
  if (!app->audio) {
    return 0;
  }

  SDL_PauseAudioDevice(app->audio, 0);

  app->quit = 0;
  app->show_help = 0;
  return 1;
}

static void handle_keyboard_note(App *app, SDL_Keycode key, int is_down) {
  int note = -1;
  switch (key) {
    // Octave 5 (QWERTY layout) - C5 to B5
    case SDLK_q: note = 72; break; // C5
    case SDLK_2: note = 73; break; // C#5
    case SDLK_w: note = 74; break; // D5
    case SDLK_3: note = 75; break; // D#5
    case SDLK_e: note = 76; break; // E5
    case SDLK_r: note = 77; break; // F5
    case SDLK_5: note = 78; break; // F#5
    case SDLK_t: note = 79; break; // G5
    case SDLK_6: note = 80; break; // G#5
    case SDLK_z: note = 81; break; // A5
    case SDLK_7: note = 82; break; // A#5
    case SDLK_u: note = 83; break; // B5

    // Octave 4 (QWERTZ layout) - C4 to B4
    case SDLK_a: note = 59; break; // C4
    case SDLK_y: note = 60; break; // C4
    case SDLK_s: note = 61; break; // C#4
    case SDLK_x: note = 62; break; // D4
    case SDLK_d: note = 63; break; // D#4
    case SDLK_c: note = 64; break; // E4
    case SDLK_f: note = 65; break; // F4
    case SDLK_v: note = 66; break; // F#4
    case SDLK_g: note = 67; break; // G4
    case SDLK_b: note = 68; break; // G#4
    case SDLK_h: note = 69; break; // A4
    case SDLK_n: note = 70; break; // A#4
    case SDLK_j: note = 71; break; // B4
    case SDLK_m: note = 72; break; // C4
    case SDLK_k: note = 73; break; // D4
  }

  if (note != -1) {
    if (is_down) {
      synth_note_on(&app->synth, note, 0.8f);
      gui_set_key_pressed(note, 1);
    } else {
      synth_note_off(&app->synth, note);
      gui_set_key_pressed(note, 0);
    }
  }
}

void app_poll_events(App *app) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT)
      app->quit = 1;
    if (e.type == SDL_KEYDOWN) {
      if (e.key.keysym.sym == SDLK_F1) {
        app->show_help = !app->show_help;
      } else if (app->show_help) {
        app->show_help = 0;
      }

      if (e.key.keysym.sym == SDLK_ESCAPE)
        app->quit = 1;
      if (e.key.keysym.sym == SDLK_SPACE) {
        arpeggiator_set_param(&app->synth.arp, "enabled",
                              !app->synth.arp.enabled);
      }
      handle_keyboard_note(app, e.key.keysym.sym, 1);
    }
    if (e.type == SDL_KEYUP) {
      handle_keyboard_note(app, e.key.keysym.sym, 0);
    }
    gui_handle_event(&app->gui, &e);
  }
}

void app_update(App *app) {
  // Calculate FPS
  app->frame_count++;
  Uint32 current_time = SDL_GetTicks();
  static Uint32 fps_last_time = 0;
  static Uint32 fps_frame_count = 0;
  
  if (fps_last_time == 0) {
    fps_last_time = current_time;
  }
  
  fps_frame_count++;
  if (current_time - fps_last_time >= 1000) { // Update FPS every second
    app->fps = fps_frame_count * 1000.0f / (current_time - fps_last_time);
    fps_last_time = current_time;
    fps_frame_count = 0;
  }
  
  gui_update(&app->gui);
  char title[256];
  snprintf(title, sizeof(title),
           "Modular Synth - Voices %d/%d | CPU %.1f%% | FPS %.1f | Comp: %.1fdB | Arp: %s",
           synth_active_voices(&app->synth), app->synth.max_voices,
           synth_cpu_usage(&app->synth), app->fps, 
           app->synth.mixer.compressor.gain_reduction,
           arpeggiator_mode_str(&app->synth.arp));
  SDL_SetWindowTitle(app->window, title);
}

void app_render_help(App *app) {
  SDL_Rect help_rect = {50, 50, 540, 380};
  SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 200);
  SDL_RenderFillRect(app->renderer, &help_rect);

  if (!app->font) {
    // Font not loaded, cannot render help text
    return;
  }

  int y = 60;
  int x = 60;
  size_t midpoint = (CC_MAP_SIZE + 1) / 2;

  for (size_t i = 0; i < midpoint; ++i) {
    char text[128];
    snprintf(text, sizeof(text), "CC %d: %s", cc_map[i].cc, cc_map[i].param);

    SDL_Color color = {255, 255, 255, 255};
    SDL_Surface *surface = TTF_RenderText_Solid(app->font, text, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(app->renderer, surface);

    SDL_Rect dst_rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(app->renderer, texture, NULL, &dst_rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);

    y += 20;
  }

  y = 60;
  x = 320;

  for (size_t i = midpoint; i < CC_MAP_SIZE; ++i) {
    char text[128];
    snprintf(text, sizeof(text), "CC %d: %s", cc_map[i].cc, cc_map[i].param);

    SDL_Color color = {255, 255, 255, 255};
    SDL_Surface *surface = TTF_RenderText_Solid(app->font, text, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(app->renderer, surface);

    SDL_Rect dst_rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(app->renderer, texture, NULL, &dst_rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);

    y += 20;
  }
}

void app_render(App *app) {
  SDL_SetRenderDrawColor(app->renderer, 18, 18, 22, 255);
  SDL_RenderClear(app->renderer);
  gui_draw(&app->gui);

  if (app->show_help) {
    app_render_help(app);
  }

  SDL_RenderPresent(app->renderer);
}

void app_shutdown(App *app) {
  SDL_CloseAudioDevice(app->audio);
  gui_shutdown(&app->gui);
  oscilloscope_shutdown();
  synth_shutdown(&app->synth);
  if (app->renderer)
    SDL_DestroyRenderer(app->renderer);
  if (app->window)
    SDL_DestroyWindow(app->window);
  TTF_Quit();
  SDL_Quit();
}
