#include "app.h"
#include <stdio.h>
#include <string.h>

int app_init(App *app) {
  memset(app, 0, sizeof(App));
  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
    return 0;

  app->window = SDL_CreateWindow("Modular Synth", SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, 640, 480,
                                 SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED);
  if (!app->window || !app->renderer)
    return 0;

  if (!synth_init(&app->synth, 48000, 1024, 64))
    return 0;
  gui_init(&app->gui, app->renderer, &app->synth);

  SDL_AudioSpec want = {0}, got = {0};
  want.freq = 48000;
  want.format = AUDIO_F32SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = synth_audio_callback;
  want.userdata = &app->synth;
  app->audio = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
  if (!app->audio)
    return 0;
  SDL_PauseAudioDevice(app->audio, 0);

  app->quit = 0;
  return 1;
}

void app_poll_events(App *app) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT)
      app->quit = 1;
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
      app->quit = 1;
    gui_handle_event(&app->gui, &e);
  }
}

void app_update(App *app) {
  gui_update(&app->gui);
  char title[256];
  snprintf(title, sizeof(title),
           "Modular Synth - Voices %d/%d | CPU %.1f%% | Arp: %s",
           synth_active_voices(&app->synth), app->synth.max_voices,
           synth_cpu_usage(&app->synth), arpeggiator_mode_str(&app->synth.arp));
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
  synth_shutdown(&app->synth);
  if (app->renderer)
    SDL_DestroyRenderer(app->renderer);
  if (app->window)
    SDL_DestroyWindow(app->window);
  SDL_Quit();
}
