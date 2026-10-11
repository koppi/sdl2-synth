#include "gui.h"
#include "synth_ui.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL_opengl.h>

// SDL2 + OpenGL glue for the standalone app. The widgets themselves live in
// synth_ui.cpp so they can be shared with the VST3 editor.

static SynthUi g_ui;
static SynthUiOptions g_options;

// Keyboard state
static int pressed_keys[128] = {0}; // 0: not pressed, 1: pressed

void gui_set_humanize_vars(int *enabled, float *vel_var, float *time_var, float *bpm, void (*toggle_func)(void), Synth *synth, ScopeBuffer *scope) {
    g_options.chord_prog_enabled = enabled;
    g_options.velocity_var = vel_var;
    g_options.timing_var = time_var;
    g_options.bpm = bpm;
    g_options.toggle_chord_progression = toggle_func;
    g_options.midi = synth ? &synth->midi : nullptr;
    g_options.scope = scope;
    g_options.show_pattern_section = true;
}

void gui_init(SDL_Window *window, SDL_GLContext gl_context) {
    // Setup Dear ImGui context
    ImGui::CreateContext(NULL);
    ImGuiIO *io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark(NULL);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 300 es");
}

void gui_shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(NULL);
}

void gui_handle_event(const SDL_Event *event) {
    ImGui_ImplSDL2_ProcessEvent(event);
}

void gui_set_key_pressed(int midi_note, int is_pressed) {
    if (midi_note >= 0 && midi_note < 128) {
        pressed_keys[midi_note] = is_pressed;
    }
}

void gui_draw(Synth *synth, SDL_Window *window, SDL_GLContext gl_context,
              int chord_prog_enabled, int current_chord_idx, int chord_prog_len,
              int current_rhythm_step, int rhythm_len) {
    (void)chord_prog_enabled;
    SDL_GL_MakeCurrent(window, gl_context);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    g_options.current_chord_idx = current_chord_idx;
    g_options.chord_prog_len = chord_prog_len;
    g_options.current_rhythm_step = current_rhythm_step;
    g_options.rhythm_len = rhythm_len;
    g_ui.draw(synth, (float)window_width, (float)window_height, g_options);

    // Rendering
    ImGui::Render();

    SDL_GL_MakeCurrent(window, gl_context);
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
