#include "app.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "oscilloscope.h"
#include "gui.h"
#include "arpeggiator.h"

static App *g_app_for_toggle = NULL;
static void toggle_chord_progression(void);
static void generate_random_chord_progression(App *app);
static void play_chord(App *app, int root_note, float velocity);
static void stop_active_notes(App *app);

int app_init(App *app) {

  memset(app, 0, sizeof(App));
  
  // Initialize timing for 60 FPS
  app->target_frame_time = 1000 / 60; // ~16.67ms per frame
  app->last_frame_time = SDL_GetTicks();
  app->frame_count = 0;
  app->fps = 0.0f;
  
  // Initialize chord progression
  app->chord_progression_enabled = 0;
  app->current_chord_index = 0;
  app->chord_progression_timer = 0.0f;
  app->chord_duration = 2.0f;
  app->chord_progression_length = 0;
  app->rhythm_pattern_length = 0;
  app->current_rhythm_step = 0;
  app->rhythm_step_timer = 0.0f;
  app->active_chord_note_count = 0;
  app->humanize_velocity_amount = 0.15f;
  app->humanize_timing_amount = 0.03f;
  app->bpm = 120.0f;

  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
    return 0;
  }

  // GL 3.0 + GLSL 130 (desktop), WebGL2/OpenGL ES 3.0 for Emscripten
#ifdef __EMSCRIPTEN__
  const char* glsl_version = "#version 300 es";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
  const char* glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

  SDL_DisplayMode dm;
  if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
    // Fallback to default resolution if getting display mode fails
    dm.w = 1280;
    dm.h = 720;
  }

  app->window = SDL_CreateWindow("sdl2-synth", SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, dm.w, dm.h,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!app->window) {
    return 0;
  }

  app->gl_context = SDL_GL_CreateContext(app->window);
  if (!app->gl_context) {
    return 0;
  }
  SDL_GL_MakeCurrent(app->window, app->gl_context);
  // Enable vsync, but handle emscripten separately
#ifndef __EMSCRIPTEN__
  SDL_GL_SetSwapInterval(1); // Enable vsync for native builds
#else
  SDL_GL_SetSwapInterval(0); // Disable vsync for emscripten to avoid timing conflicts
#endif

  if (!synth_init(&app->synth, 48000, 1024, 16)) {
    return 0;
  }

  gui_init(app->window, app->gl_context);
  g_app_for_toggle = app;
  gui_set_humanize_vars(&app->chord_progression_enabled, &app->humanize_velocity_amount, &app->humanize_timing_amount, &app->bpm, toggle_chord_progression, &app->synth);

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
  int base_note = -1;
  
  // Determine base note (in octave 0) without octave offset
  switch (key) {
    // Base notes (relative to octave 0)
    case SDLK_q: base_note = 0; break; // C
    case SDLK_2: base_note = 1; break; // C#
    case SDLK_w: base_note = 2; break; // D
    case SDLK_3: base_note = 3; break; // D#
    case SDLK_e: base_note = 4; break; // E
    case SDLK_r: base_note = 5; break; // F
    case SDLK_5: base_note = 6; break; // F#
    case SDLK_t: base_note = 7; break; // G
    case SDLK_6: base_note = 8; break; // G#
    case SDLK_z: base_note = 9; break; // A
    case SDLK_7: base_note = 10; break; // A#
    case SDLK_u: base_note = 11; break; // B

    case SDLK_a: base_note = 0; break; // C (second octave)
    case SDLK_y: base_note = 0; break; // C (second octave)
    case SDLK_s: base_note = 1; break; // C# (second octave)
    case SDLK_x: base_note = 2; break; // D (second octave)
    case SDLK_d: base_note = 3; break; // D# (second octave)
    case SDLK_c: base_note = 4; break; // E (second octave)
    case SDLK_f: base_note = 5; break; // F (second octave)
    case SDLK_v: base_note = 6; break; // F# (second octave)
    case SDLK_g: base_note = 7; break; // G (second octave)
    case SDLK_b: base_note = 8; break; // G# (second octave)
    case SDLK_h: base_note = 9; break; // A (second octave)
    case SDLK_n: base_note = 10; break; // A# (second octave)
    case SDLK_j: base_note = 11; break; // B (second octave)
    case SDLK_m: base_note = 0; break; // C (third octave)
    case SDLK_k: base_note = 2; break; // D (third octave)
  }
  
  // Apply octave offset (octave 0-4 corresponds to MIDI octaves 1-5)
  if (base_note != -1) {
    note = base_note + (app->synth.arp.octave * 12);
  }

  if (note != -1) {
    if (is_down) {
      if (app->synth.arp.enabled) {
        arpeggiator_note_on(&app->synth.arp, note);
      } else {
        synth_note_on(&app->synth, note, 0.8f);
      }
      gui_set_key_pressed(note, 1);
    } else {
      if (app->synth.arp.enabled) {
        arpeggiator_note_off(&app->synth.arp, note);
      } else {
        synth_note_off(&app->synth, note);
      }
      gui_set_key_pressed(note, 0);
    }
  }
}

static void toggle_chord_progression(void) {
  if (g_app_for_toggle) {
    g_app_for_toggle->chord_progression_enabled = !g_app_for_toggle->chord_progression_enabled;
    if (g_app_for_toggle->chord_progression_enabled) {
      generate_random_chord_progression(g_app_for_toggle);
      play_chord(g_app_for_toggle, g_app_for_toggle->chord_progression[0], 
                 g_app_for_toggle->rhythm_pattern[0]);
    } else {
      stop_active_notes(g_app_for_toggle);
    }
  }
}

static void generate_random_chord_progression(App *app) {
  int scale[] = {0, 2, 4, 5, 7, 9, 11};
  int progression_patterns[][8] = {
    {0, 5, 3, 4, 2, 6, 1, 0},
    {0, 3, 5, 4, 2, 1, 6, 5},
    {0, 4, 5, 3, 1, 6, 2, 0},
    {0, 5, 3, 4, 0, 5, 3, 6},
    {0, 3, 4, 5, 1, 2, 6, 0},
    {0, 4, 3, 5, 6, 1, 2, 4},
    {0, 6, 3, 5, 4, 1, 2, 0},
    {0, 2, 5, 3, 6, 4, 1, 0},
  };
  
  int pattern_idx = rand() % 8;
  int root = 48 + (rand() % 12);
  
  app->chord_progression_length = 8;
  for (int i = 0; i < 8; i++) {
    app->chord_progression[i] = root + scale[progression_patterns[pattern_idx][i]];
  }
  
  // Generate 128-step rhythm pattern by repeating 16-step patterns
  float base_patterns[][16] = {
    {1.0f, 0.0f, 0.5f, 0.0f, 0.75f, 0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.75f, 0.0f, 0.5f, 0.0f},
    {1.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.75f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.75f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.5f, 0.5f, 0.0f, 0.75f, 0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 0.75f, 0.0f, 0.5f, 0.0f},
    {1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.75f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.75f, 0.0f, 0.0f},
    {1.0f, 0.5f, 0.0f, 0.5f, 0.75f, 0.5f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, 0.5f, 0.75f, 0.5f, 0.0f, 0.5f},
  };
  
  int rhythm_idx = rand() % 5;
  app->rhythm_pattern_length = 128;
  
  // Fill 128 steps by repeating the 16-step pattern 8 times
  for (int i = 0; i < 128; i++) {
    app->rhythm_pattern[i] = base_patterns[rhythm_idx][i % 16];
  }
  
  app->current_chord_index = 0;
  app->chord_progression_timer = 0.0f;
  app->current_rhythm_step = 0;
  app->rhythm_step_timer = 0.0f;
}

static float humanize_velocity(float velocity, float amount) {
  float random = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
  velocity += random * amount;
  if (velocity < 0.1f) velocity = 0.1f;
  if (velocity > 1.0f) velocity = 1.0f;
  return velocity;
}

static void stop_active_notes(App *app) {
  for (int i = 0; i < app->active_chord_note_count; i++) {
    synth_note_off(&app->synth, app->active_chord_notes[i]);
  }
  app->active_chord_note_count = 0;
}

static void play_chord(App *app, int root_note, float velocity) {
  if (velocity <= 0.0f) {
    return;
  }
  
  velocity = humanize_velocity(velocity, app->humanize_velocity_amount);
  
  int chord_notes[8];
  chord_notes[0] = root_note;
  chord_notes[1] = root_note + 3;
  chord_notes[2] = root_note + 4;
  chord_notes[3] = root_note + 7;
  chord_notes[4] = root_note + 10;
  chord_notes[5] = root_note + 12;
  chord_notes[6] = root_note + 15;
  chord_notes[7] = root_note + 16;
  
  app->active_chord_note_count = 0;
  for (int i = 0; i < 8; i++) {
    float note_velocity = humanize_velocity(velocity, app->humanize_velocity_amount);
    synth_note_on(&app->synth, chord_notes[i], note_velocity);
    app->active_chord_notes[app->active_chord_note_count++] = chord_notes[i];
  }
}

void app_poll_events(App *app) {
  SDL_Event e;
  int event_count = 0;
  while (SDL_PollEvent(&e)) {
    event_count++;
    gui_handle_event(&e);
    if (e.type == SDL_QUIT) {
      app->quit = 1;
    }
    if (e.type == SDL_KEYDOWN) {
      if (e.key.keysym.sym == SDLK_F1) {
        app->chord_progression_enabled = !app->chord_progression_enabled;
        if (app->chord_progression_enabled) {
          generate_random_chord_progression(app);
          play_chord(app, app->chord_progression[0], app->rhythm_pattern[0]);
        } else {
          stop_active_notes(app);
        }
      } else if (app->show_help) {
        app->show_help = 0;
      }

      if (e.key.keysym.sym == SDLK_ESCAPE) {
        app->quit = 1;
      }
      if (e.key.keysym.sym == SDLK_SPACE) {
        arpeggiator_set_param(&app->synth.arp, "enabled",
                              !app->synth.arp.enabled, &app->synth);
      }
      handle_keyboard_note(app, e.key.keysym.sym, 1);
    }
    if (e.type == SDL_KEYUP) {
      handle_keyboard_note(app, e.key.keysym.sym, 0);
    }
  }
  if (event_count > 0) {
  }
}

void app_update(App *app) {
  
  // Update chord progression with rhythm
  if (app->chord_progression_enabled && app->chord_progression_length > 0 && app->rhythm_pattern_length > 0) {
    float delta_time = app->target_frame_time / 1000.0f;
    float seconds_per_beat = 60.0f / app->bpm;
    float step_duration = seconds_per_beat * 4.0f / app->rhythm_pattern_length;
    
    float timing_variation = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * app->humanize_timing_amount;
    step_duration += timing_variation;
    if (step_duration < 0.01f) step_duration = 0.01f;
    
    app->rhythm_step_timer += delta_time;
    
    if (app->rhythm_step_timer >= step_duration) {
      app->rhythm_step_timer -= step_duration;
      
      stop_active_notes(app);
      
      app->current_rhythm_step = (app->current_rhythm_step + 1) % app->rhythm_pattern_length;
      
      if (app->current_rhythm_step == 0) {
        app->current_chord_index = (app->current_chord_index + 1) % app->chord_progression_length;
      }
      
      float velocity = app->rhythm_pattern[app->current_rhythm_step];
      play_chord(app, app->chord_progression[app->current_chord_index], velocity);
    }
  }
  
  // Sync FX BPM with chord progression BPM
  synth_set_bpm(&app->synth, app->bpm);
  
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
  
  char title[256];
   snprintf(title, sizeof(title),
            "Voices %d/%d | CPU %.1f%% | FPS %.1f | Comp: %.1fdB | Arp: %s | Oct: %d | Octaves: %d",
            synth_active_voices(&app->synth), app->synth.max_voices,
            synth_cpu_usage(&app->synth), app->fps, 
            app->synth.mixer.compressor.gain_reduction,
            arpeggiator_mode_str(&app->synth.arp), app->synth.arp.octave + 1, app->synth.arp.octaves);
  SDL_SetWindowTitle(app->window, title);
  
}

void app_render(App *app) {
    gui_draw(&app->synth, app->window, app->gl_context, 
             app->chord_progression_enabled, app->current_chord_index, 
             app->chord_progression_length, app->current_rhythm_step, 
             app->rhythm_pattern_length);
    SDL_GL_SwapWindow(app->window);
}

void app_shutdown(App *app) {
  // First pause and close audio to stop any callbacks
  SDL_PauseAudioDevice(app->audio, 1);
  SDL_Delay(50); // Allow audio callbacks to complete
  SDL_CloseAudioDevice(app->audio);
  
  // Then shutdown other components
  gui_shutdown();
  oscilloscope_shutdown();
  synth_shutdown(&app->synth);
  SDL_GL_DeleteContext(app->gl_context);
  if (app->window)
    SDL_DestroyWindow(app->window);
  SDL_Quit();
}
