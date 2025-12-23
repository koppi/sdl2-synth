#include "app.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

#if __EMSCRIPTEN__
	#include <emscripten.h>
#endif

App app;

void main_loop() {
#ifdef __EMSCRIPTEN__
    // Set timing on first iteration to avoid warnings
    static bool timing_set = false;
    if (!timing_set) {
        emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
        timing_set = true;
    }
    
    app_poll_events(&app);
    app_update(&app);
    app_render(&app);
#else
    // For native builds, use manual frame rate limiting
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
#endif
}

int main(int argc, char *argv[]) {
  if (!app_init(&app)) {
    return 1;
  }

#if __EMSCRIPTEN__
  // Set up main loop after all initialization is complete  
  emscripten_set_main_loop(main_loop, 0, 1);
  // Note: emscripten_set_main_loop_timing called from main_loop to avoid timing warnings
#else
  while (!app.quit) {
	  main_loop();
  }
#endif

  app_shutdown(&app);
  return 0;
}
