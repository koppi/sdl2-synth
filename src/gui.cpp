#include "gui.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "oscilloscope.h"
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL_opengl.h>
#include <GL/gl.h>

// Keyboard state
static int pressed_keys[128] = {0}; // 0: not pressed, 1: pressed

static SDL_Renderer *g_renderer = NULL;
static TTF_Font *g_font = NULL;

static GLuint g_oscilloscope_gl_texture = 0;
static SDL_Surface* g_oscilloscope_surface = NULL;
static SDL_Renderer* g_oscilloscope_renderer = NULL;
static int osc_width = 500;
static int osc_height = 300;

void gui_init(SDL_Window *window, SDL_GLContext gl_context) {
    // Setup Dear ImGui context
    ImGui::CreateContext(NULL);
    ImGuiIO *io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark(NULL);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    g_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        fprintf(stderr, "Failed to create SDL renderer: %s\n", SDL_GetError());
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "Failed to initialize SDL_ttf: %s\n", TTF_GetError());
    }

    g_font = TTF_OpenFont("DejaVuSans.ttf", 12);
    if (!g_font) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
    }

    oscilloscope_init();

    // Initialize persistent oscilloscope rendering resources
    g_oscilloscope_surface = SDL_CreateRGBSurfaceWithFormat(0, osc_width, osc_height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!g_oscilloscope_surface) {
        fprintf(stderr, "Failed to create g_oscilloscope_surface: %s\n", SDL_GetError());
    } else {
        g_oscilloscope_renderer = SDL_CreateSoftwareRenderer(g_oscilloscope_surface);
        if (!g_oscilloscope_renderer) {
            fprintf(stderr, "Failed to create g_oscilloscope_renderer: %s\n", SDL_GetError());
            SDL_FreeSurface(g_oscilloscope_surface);
            g_oscilloscope_surface = NULL;
        }
    }

    glGenTextures(1, &g_oscilloscope_gl_texture);
    glBindTexture(GL_TEXTURE_2D, g_oscilloscope_gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, osc_width, osc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); // Allocate texture memory
}

void gui_shutdown() {
    oscilloscope_shutdown();

    if (g_oscilloscope_gl_texture) {
        glDeleteTextures(1, &g_oscilloscope_gl_texture);
        g_oscilloscope_gl_texture = 0;
    }
    if (g_oscilloscope_renderer) {
        SDL_DestroyRenderer(g_oscilloscope_renderer);
        g_oscilloscope_renderer = NULL;
    }
    if (g_oscilloscope_surface) {
        SDL_FreeSurface(g_oscilloscope_surface);
        g_oscilloscope_surface = NULL;
    }

    if (g_font) {
        TTF_CloseFont(g_font);
        g_font = NULL;
    }
    TTF_Quit();

    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }

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

void gui_draw(Synth *synth, SDL_Window *window, SDL_GLContext gl_context) {
    SDL_GL_MakeCurrent(window, gl_context);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)window_width, (float)window_height), ImGuiCond_Always);
    ImGui::Begin("Synth", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Oscillators
    if (ImGui::CollapsingHeader("Oscillators", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "osc_columns", true);
        for (int i = 0; i < 6; ++i) {
            ImGui::PushID(i);
            char title[32];
            sprintf(title, "OSC %d", i + 1);
            if (i == 4) sprintf(title, "BASS");
            if (i == 5) sprintf(title, "PERC");

            ImGui::BeginChild(title, ImVec2(0, 200), true, 0);
            ImGui::Text("%s", title);

            const char* items[] = { "SINE", "SAW", "SQUARE", "TRI", "NOISE" };
            int current_item = (int)synth->osc[i].waveform;
            if (ImGui::Combo("Waveform", &current_item, "SINE\0SAW\0SQUARE\0TRI\0NOISE\0\0", 5)) {
                synth->osc[i].waveform = (OscWaveform)current_item;
            }

            ImGui::SliderFloat("Pitch", &synth->osc[i].pitch, -24.0f, 24.0f, "%.2f", 0);
            ImGui::SliderFloat("Detune", &synth->osc[i].detune, -1.0f, 1.0f, "%.2f", 0);
            ImGui::SliderFloat("Gain", &synth->osc[i].gain, 0.0f, 1.0f, "%.2f", 0);
            ImGui::SliderFloat("Pulse Width", &synth->osc[i].pulse_width, 0.0f, 1.0f, "%.2f", 0);
            int unison_voices = synth->osc[i].unison_voices;
            if (ImGui::SliderInt("Unison", &unison_voices, 1, 8, "%d", 0)) {
                synth->osc[i].unison_voices = unison_voices;
            }
            ImGui::SliderFloat("Unison Detune", &synth->osc[i].unison_detune, 0.0f, 1.0f, "%.2f", 0);

            ImGui::EndChild();
            ImGui::PopID();
            if (i % 2 != 0) {
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1, "", false);
    }

    // Effects
    if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(3, "fx_columns", true);
        ImGui::SetColumnWidth(0, (float)window_width / 3.0f);
        ImGui::SetColumnWidth(1, (float)window_width / 3.0f);
        ImGui::SetColumnWidth(2, (float)window_width / 3.0f);

        ImGui::Text("Flanger");
        ImGui::SliderFloat("Depth##flanger", &synth->fx.flanger_depth, 0.0f, 1.0f, "%.2f", 0);
        ImGui::SliderFloat("Rate##flanger", &synth->fx.flanger_rate, 0.0f, 5.0f, "%.2f", 0);
        ImGui::SliderFloat("Feedback##flanger", &synth->fx.flanger_feedback, 0.0f, 1.0f, "%.2f", 0);
        ImGui::NextColumn();

        ImGui::Text("Delay");
        ImGui::SliderFloat("Time##delay", &synth->fx.delay_time, 0.0f, 1.0f, "%.2f", 0);
        ImGui::SliderFloat("Feedback##delay", &synth->fx.delay_feedback, 0.0f, 1.0f, "%.2f", 0);
        ImGui::SliderFloat("Mix##delay", &synth->fx.delay_mix, 0.0f, 1.0f, "%.2f", 0);
        ImGui::NextColumn();

        ImGui::Text("Reverb");
        ImGui::SliderFloat("Size##reverb", &synth->fx.reverb_size, 0.0f, 1.0f, "%.2f", 0);
        ImGui::SliderFloat("Mix##reverb", &synth->fx.reverb_mix, 0.0f, 1.0f, "%.2f", 0);
        ImGui::SliderFloat("Damping##reverb", &synth->fx.reverb_damping, 0.0f, 1.0f, "%.2f", 0);

        ImGui::Columns(1, "", false);
    }

    // Mixer
    if (ImGui::CollapsingHeader("Mixer", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(1, "mixer_columns", true);
        for (int i = 0; i < 4; ++i) {
            char label[16];
            sprintf(label, "OSC%d", i + 1);
            ImGui::SliderFloat(label, &synth->mixer.osc_gain[i], 0.0f, 1.0f, "%.2f", 0);
            ImGui::NextColumn();
        }
        ImGui::SliderFloat("Master", &synth->mixer.master, 0.0f, 2.0f, "%.2f", 0);
        ImGui::NextColumn();

        ImGui::Checkbox("Compressor", (bool*)&synth->mixer.comp_enabled);
        ImGui::SliderFloat("Threshold", &synth->mixer.comp_threshold, -24.0f, 0.0f, "%.2f", 0);
        ImGui::SliderFloat("Ratio", &synth->mixer.comp_ratio, 1.0f, 10.0f, "%.2f", 0);
        ImGui::SliderFloat("Attack", &synth->mixer.comp_attack, 1.0f, 100.0f, "%.2f", 0);
        ImGui::SliderFloat("Release", &synth->mixer.comp_release, 10.0f, 1000.0f, "%.2f", 0);
        ImGui::SliderFloat("Makeup", &synth->mixer.comp_makeup_gain, 0.0f, 12.0f, "%.2f", 0);

        ImGui::Columns(1, "", false);
    }

    // Arpeggiator
    if (ImGui::CollapsingHeader("Arpeggiator", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled##arp", (bool*)&synth->arp.enabled);
        const char* arp_modes[] = { "UP", "DOWN", "ORDER", "RANDOM" };
        int current_arp_mode = (int)synth->arp.mode;
        if (ImGui::Combo("Mode", &current_arp_mode, "UP\0DOWN\0ORDER\0RANDOM\0\0", 4)) {
            synth->arp.mode = (ArpMode)current_arp_mode;
        }
        ImGui::SliderFloat("Tempo", &synth->arp.tempo, 30.0f, 240.0f, "%.2f", 0);
        ImGui::Checkbox("Polyphonic", (bool*)&synth->arp.polyphonic);
    }

    // Oscilloscope
    ImGui::Begin("Oscilloscope");
    ImVec2 content_size = ImGui::GetContentRegionAvail();
    if (content_size.x > 0 && content_size.y > 0) {
        if ((int)content_size.x != osc_width || (int)content_size.y != osc_height) {
            // Recreate resources if size has changed
            if (g_oscilloscope_renderer) SDL_DestroyRenderer(g_oscilloscope_renderer);
            if (g_oscilloscope_surface) SDL_FreeSurface(g_oscilloscope_surface);
            if (g_oscilloscope_gl_texture) glDeleteTextures(1, &g_oscilloscope_gl_texture);

            osc_width = (int)content_size.x;
            osc_height = (int)content_size.y;

            g_oscilloscope_surface = SDL_CreateRGBSurfaceWithFormat(0, osc_width, osc_height, 32, SDL_PIXELFORMAT_RGBA32);
            g_oscilloscope_renderer = SDL_CreateSoftwareRenderer(g_oscilloscope_surface);
            
            glGenTextures(1, &g_oscilloscope_gl_texture);
            glBindTexture(GL_TEXTURE_2D, g_oscilloscope_gl_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, osc_width, osc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        }

        if (g_oscilloscope_renderer && g_oscilloscope_surface && g_oscilloscope_gl_texture && g_font) {
            SDL_SetRenderDrawColor(g_oscilloscope_renderer, 0, 0, 0, 0);
            SDL_RenderClear(g_oscilloscope_renderer);
            oscilloscope_draw(g_oscilloscope_renderer, synth, 0, 0, osc_width, osc_height, g_font);

            glBindTexture(GL_TEXTURE_2D, g_oscilloscope_gl_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, osc_width, osc_height, GL_RGBA, GL_UNSIGNED_BYTE, g_oscilloscope_surface->pixels);

            ImGui::Image((void*)(intptr_t)g_oscilloscope_gl_texture, ImVec2((float)osc_width, (float)osc_height));
        }
    }
    ImGui::End();

    ImGui::End();

    // Rendering
    ImGui::Render();

    SDL_GL_MakeCurrent(window, gl_context);
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
