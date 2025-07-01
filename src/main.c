#include "app.h"

int main(int argc, char *argv[]) {
    App app;
    if (!app_init(&app)) return 1;

    while (!app.quit) {
        app_poll_events(&app);
        app_update(&app);
        app_render(&app);
    }

    app_shutdown(&app);
    return 0;
}