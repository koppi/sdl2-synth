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
    printf("Failed to initialize SDL: %s\n", SDL_GetError());
    return 0;
  }
  printf("SDL initialized successfully\n");

  app->window = SDL_CreateWindow("Modular Synth", SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, 640, 480,
                                 SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!app->window) {
    printf("Failed to create window: %s\n", SDL_GetError());
    return 0;
  }
  printf("Window created successfully\n");

  app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED);
  if (!app->renderer) {
    printf("Failed to create renderer: %s\n", SDL_GetError());
    return 0;
  }
  printf("Renderer created successfully\n");

  if (!synth_init(&app->synth, 48000, 1024, 64)) {
    printf("Failed to initialize synth\n");
    return 0;
  }
  printf("Synth initialized successfully\n");

  gui_init(&app->gui, app->renderer, &app->synth);
  oscilloscope_init();
  printf("GUI initialized successfully\n");

  SDL_AudioSpec want = {0}, got = {0};
  want.freq = 48000;
  want.format = AUDIO_F32SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = synth_audio_callback;
  want.userdata = &app->synth;
  app->audio = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
  if (!app->audio) {
    printf("Failed to open audio device: %s\n", SDL_GetError());
    return 0;
  }
  printf("Audio device opened successfully\n");

  SDL_PauseAudioDevice(app->audio, 0);

  // Startup melody disabled - synthesizer starts silently
  // synth_start_melody(&app->synth);  // Commented out to disable startup melody

  app->quit = 0;
  return 1;
}

void app_poll_events(App *app) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT)
      app->quit = 1;
    if (e.type == SDL_KEYDOWN) {
      if (e.key.keysym.sym == SDLK_ESCAPE)
        app->quit = 1;
      if (e.key.keysym.sym == SDLK_SPACE) {
        arpeggiator_set_param(&app->synth.arp, "enabled",
                              !app->synth.arp.enabled);
      }
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

void app_render(App *app) {
  SDL_SetRenderDrawColor(app->renderer, 18, 18, 22, 255);
  SDL_RenderClear(app->renderer);
  gui_draw(&app->gui);
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
  SDL_Quit();
}
