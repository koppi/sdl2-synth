#include "app.h"
#include <SDL_timer.h>

int main(int argc, char *argv[]) {
  App app;
  if (!app_init(&app))
    return 1;

  while (!app.quit) {
    Uint32 frame_start = SDL_GetTicks();
    
    app_poll_events(&app);
    app_update(&app);
    app_render(&app);
    
    // Frame rate limiting for 60 FPS
    Uint32 frame_time = SDL_GetTicks() - frame_start;
    if (frame_time < app.target_frame_time) {
      SDL_Delay(app.target_frame_time - frame_time);
    }
    
    app.last_frame_time = SDL_GetTicks();
  }

  app_shutdown(&app);
  return 0;
}