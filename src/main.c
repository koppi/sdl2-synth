#include "app.h"
#include <SDL_timer.h>


#if __EMSCRIPTEN__
	#include <emscripten.h>
#endif

App app;

void main_loop() {
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

int main(int argc, char *argv[]) {
  if (!app_init(&app))
    return 1;

#if __EMSCRIPTEN__
  emscripten_set_main_loop(main_loop, 60, 1);
#else
  while (!app.quit) {
	  main_loop();
  }
#endif

  app_shutdown(&app);
  return 0;
}
