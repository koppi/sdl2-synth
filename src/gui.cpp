#include "gui.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "oscilloscope.h"
#include "dejavusans_ttf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <SDL_opengl.h>
// #include <GL/gl.h>
// #include <OpenGL/gl.h>

// Global pointers to app state for GUI access
static int *g_chord_prog_enabled = NULL;
static float *g_velocity_var = NULL;
static float *g_timing_var = NULL;
static float *g_bpm = NULL;
static Synth *g_synth = NULL;
static void (*g_toggle_chord_progression)(void) = NULL;

// Keyboard state
static int pressed_keys[128] = {0}; // 0: not pressed, 1: pressed

static SDL_Renderer *g_renderer = NULL;
static TTF_Font *g_font = NULL;

static GLuint g_oscilloscope_gl_texture = 0;
static SDL_Surface* g_oscilloscope_surface = NULL;
static SDL_Renderer* g_oscilloscope_renderer = NULL;
static int osc_width = 500;
static int osc_height = 300;

void gui_set_humanize_vars(int *enabled, float *vel_var, float *time_var, float *bpm, void (*toggle_func)(void), Synth *synth) {
    g_chord_prog_enabled = enabled;
    g_velocity_var = vel_var;
    g_timing_var = time_var;
    g_bpm = bpm;
    g_toggle_chord_progression = toggle_func;
    g_synth = synth;
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

    g_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        fprintf(stderr, "Failed to create SDL renderer: %s\n", SDL_GetError());
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "Failed to initialize SDL_ttf: %s\n", TTF_GetError());
    }

    // Load font from embedded data
    SDL_RWops *font_rw = SDL_RWFromConstMem(DejaVuSans_ttf, DejaVuSans_ttf_len);
    if (!font_rw) {
        fprintf(stderr, "Failed to create font RWops: %s\n", SDL_GetError());
    }
    
    g_font = TTF_OpenFontRW(font_rw, 1, 12); // 1 = auto-close RWops
    if (!g_font) {
        fprintf(stderr, "Failed to load embedded font: %s\n", TTF_GetError());
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

void gui_draw(Synth *synth, SDL_Window *window, SDL_GLContext gl_context, 
              int chord_prog_enabled, int current_chord_idx, int chord_prog_len,
              int current_rhythm_step, int rhythm_len) {
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

    // Oscilloscope
    if (ImGui::CollapsingHeader("Oscilloscope", ImGuiTreeNodeFlags_DefaultOpen)) {
		// Set fixed height of 200 pixels, use full available width
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
		ImGui::BeginChild("OscilloscopeChild", ImVec2(0, 200), false, ImGuiWindowFlags_NoScrollbar);
		ImVec2 content_size = ImGui::GetContentRegionAvail();
		if (content_size.x > 0 && content_size.y > 0) {
			if ((int)content_size.x != osc_width || 200 != osc_height) {
				// Recreate resources if size has changed
				if (g_oscilloscope_renderer) SDL_DestroyRenderer(g_oscilloscope_renderer);
				if (g_oscilloscope_surface) SDL_FreeSurface(g_oscilloscope_surface);
				if (g_oscilloscope_gl_texture) glDeleteTextures(1, &g_oscilloscope_gl_texture);
				
				osc_width = (int)content_size.x;
				osc_height = 200; // Fixed height of 200 pixels
				
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
		ImGui::EndChild();
		ImGui::PopStyleVar();
	}

    // Oscillators
    if (ImGui::CollapsingHeader("Oscillators", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(3, "osc_columns", true);
        for (int i = 0; i < 6; ++i) {
            ImGui::PushID(i);
            char title[32];
            sprintf(title, "OSC %d", i + 1);

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
            ImGui::SliderFloat("Pan", &synth->osc[i].pan, -1.0f, 1.0f, "%.2f", 0);
            ImGui::SliderFloat("Pulse Width", &synth->osc[i].pulse_width, 0.0f, 1.0f, "%.2f", 0);
            int unison_voices = synth->osc[i].unison_voices;
            if (ImGui::SliderInt("Unison", &unison_voices, 1, 8, "%d", 0)) {
                synth->osc[i].unison_voices = unison_voices;
            }
            ImGui::SliderFloat("Unison Detune", &synth->osc[i].unison_detune, 0.0f, 1.0f, "%.2f", 0);

            ImGui::EndChild();
            ImGui::PopID();
			ImGui::NextColumn();
        }
        ImGui::Columns(1, "", false);
        
        // Oscillator randomization buttons
        ImGui::Separator();
        
        // Randomize All button spanning full width
        if (ImGui::Button("Randomize All OSCs", ImVec2(-1, 0))) {
            synth_randomize_parameters(synth);
        }
        
        // Individual oscillator randomization buttons
        ImGui::Columns(6, "random_buttons", true);
        
        if (ImGui::Button("Randomize OSC 1")) {
            synth_randomize_oscillator(synth, 0);
        }
        ImGui::NextColumn();
        if (ImGui::Button("Randomize OSC 2")) {
            synth_randomize_oscillator(synth, 1);
        }
        ImGui::NextColumn();
        if (ImGui::Button("Randomize OSC 3")) {
            synth_randomize_oscillator(synth, 2);
        }
        ImGui::NextColumn();
        if (ImGui::Button("Randomize OSC 4")) {
            synth_randomize_oscillator(synth, 3);
        }
        ImGui::NextColumn();
        if (ImGui::Button("Randomize OSC 5")) {
            synth_randomize_oscillator(synth, 4);
        }
        ImGui::NextColumn();
        if (ImGui::Button("Randomize OSC 6")) {
            synth_randomize_oscillator(synth, 5);
        }
		ImGui::Columns(1, "", false);
    }

    // ADSR Envelope
    if (ImGui::CollapsingHeader("ADSR Envelope", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 4x1 layout with envelope visualization
        ImGui::Columns(5, "adsr_columns", true); // 4 parameters + 1 for envelope visualization
        
        // Attack column
        ImGui::Text("Attack");
        if (ImGui::SliderFloat("##adsrattack", &synth->adsr.attack, 0.001f, 5.0f, "%.3f s", 0)) {
            synth_set_param(synth, "adsr.attack", synth->adsr.attack);
        }
        ImGui::NextColumn();
        
        // Decay column
        ImGui::Text("Decay");
        if (ImGui::SliderFloat("##adsrdecay", &synth->adsr.decay, 0.001f, 5.0f, "%.3f s", 0)) {
            synth_set_param(synth, "adsr.decay", synth->adsr.decay);
        }
        ImGui::NextColumn();
        
        // Sustain column
        ImGui::Text("Sustain");
        if (ImGui::SliderFloat("##adsrsustain", &synth->adsr.sustain, 0.0f, 1.0f, "%.2f", 0)) {
            synth_set_param(synth, "adsr.sustain", synth->adsr.sustain);
        }
        ImGui::NextColumn();
        
        // Release column
        ImGui::Text("Release");
        if (ImGui::SliderFloat("##adsrrelease", &synth->adsr.release, 0.001f, 10.0f, "%.3f s", 0)) {
            synth_set_param(synth, "adsr.release", synth->adsr.release);
        }
        ImGui::NextColumn();
        
        // Envelope visualization column
        ImGui::Text("Envelope");
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImVec2(120, 80); // Fixed size for envelope display
        
        // Draw background
        ImVec2 bg_end = ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);
        draw_list->AddRectFilled(canvas_pos, bg_end, IM_COL32(20, 25, 35, 255));
        draw_list->AddRect(canvas_pos, bg_end, IM_COL32(100, 100, 120, 255));
        
        // Calculate envelope curve points
        const int num_points = 100;
        float total_time = synth->adsr.attack + synth->adsr.decay + synth->adsr.release + 1.0f; // Add 1 second for sustain phase
        float max_time = fmaxf(total_time, 2.0f); // At least 2 seconds for display
        
        // Calculate envelope points and clip to canvas bounds
        ImVec2 points[num_points];
        int valid_points = 0;
        
        for (int i = 0; i < num_points; i++) {
            float t = (float)i / (float)(num_points - 1) * max_time;
            float amplitude = 0.0f;
            
            if (t < synth->adsr.attack) {
                // Attack phase: linear ramp from 0 to 1
                amplitude = t / synth->adsr.attack;
            } else if (t < synth->adsr.attack + synth->adsr.decay) {
                // Decay phase: exponential decay from 1 to sustain
                float decay_progress = (t - synth->adsr.attack) / synth->adsr.decay;
                amplitude = 1.0f - (1.0f - synth->adsr.sustain) * decay_progress;
            } else if (t < synth->adsr.attack + synth->adsr.decay + 1.0f) {
                // Sustain phase: constant
                amplitude = synth->adsr.sustain;
            } else {
                // Release phase: exponential decay from sustain to 0
                float release_progress = (t - synth->adsr.attack - synth->adsr.decay - 1.0f) / synth->adsr.release;
                amplitude = synth->adsr.sustain * (1.0f - release_progress);
            }
            
            // Calculate screen position and clip to canvas bounds
            float x = canvas_pos.x + (float)i / (float)(num_points - 1) * canvas_size.x;
            float y = canvas_pos.y + canvas_size.y - amplitude * canvas_size.y;
            
            // Clamp coordinates to canvas bounds
            x = fmaxf(canvas_pos.x, fminf(x, canvas_pos.x + canvas_size.x));
            y = fmaxf(canvas_pos.y, fminf(y, canvas_pos.y + canvas_size.y));
            
            points[i] = ImVec2(x, y);
            valid_points++;
        }
        
        // Draw clipped polyline using the calculated points
        if (valid_points > 1) {
            draw_list->AddPolyline(points, valid_points, IM_COL32(100, 255, 100, 255), 0, 2.0f);
        }
        
        // Draw phase labels
        draw_list->AddText(ImVec2(canvas_pos.x + 2, canvas_pos.y + 2), IM_COL32(180, 180, 200, 255), "A");
        draw_list->AddText(ImVec2(canvas_pos.x + canvas_size.x / 4 + 2, canvas_pos.y + canvas_size.y - 15), IM_COL32(180, 180, 200, 255), "D");
        draw_list->AddText(ImVec2(canvas_pos.x + canvas_size.x / 2 + 2, canvas_pos.y + canvas_size.y - 15), IM_COL32(180, 180, 200, 255), "S");
        draw_list->AddText(ImVec2(canvas_pos.x + 3 * canvas_size.x / 4 + 2, canvas_pos.y + canvas_size.y - 15), IM_COL32(180, 180, 200, 255), "R");
        
        ImGui::Dummy(canvas_size); // Reserve space for the drawing
        
        ImGui::Columns(1, "", false);
        
        // Preset buttons
        ImGui::Separator();
        ImGui::Columns(7, "adsr_presets", true);
        
        if (ImGui::Button("Pad")) {
            synth_set_param(synth, "adsr.attack", 1.5f);
            synth_set_param(synth, "adsr.decay", 0.8f);
            synth_set_param(synth, "adsr.sustain", 0.7f);
            synth_set_param(synth, "adsr.release", 2.0f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Pluck")) {
            synth_set_param(synth, "adsr.attack", 0.001f);
            synth_set_param(synth, "adsr.decay", 0.5f);
            synth_set_param(synth, "adsr.sustain", 0.3f);
            synth_set_param(synth, "adsr.release", 0.8f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Lead")) {
            synth_set_param(synth, "adsr.attack", 0.05f);
            synth_set_param(synth, "adsr.decay", 0.2f);
            synth_set_param(synth, "adsr.sustain", 0.8f);
            synth_set_param(synth, "adsr.release", 0.3f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Bass")) {
            synth_set_param(synth, "adsr.attack", 0.01f);
            synth_set_param(synth, "adsr.decay", 0.1f);
            synth_set_param(synth, "adsr.sustain", 0.9f);
            synth_set_param(synth, "adsr.release", 0.1f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Ambient")) {
            synth_set_param(synth, "adsr.attack", 3.0f);
            synth_set_param(synth, "adsr.decay", 1.5f);
            synth_set_param(synth, "adsr.sustain", 0.6f);
            synth_set_param(synth, "adsr.release", 4.0f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Staccato")) {
            synth_set_param(synth, "adsr.attack", 0.001f);
            synth_set_param(synth, "adsr.decay", 0.05f);
            synth_set_param(synth, "adsr.sustain", 0.0f);
            synth_set_param(synth, "adsr.release", 0.1f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Strings")) {
            synth_set_param(synth, "adsr.attack", 0.8f);
            synth_set_param(synth, "adsr.decay", 0.3f);
            synth_set_param(synth, "adsr.sustain", 0.8f);
            synth_set_param(synth, "adsr.release", 1.2f);
        }
        ImGui::NextColumn();
        
        // Row 2 - New presets
        if (ImGui::Button("Piano")) {
            synth_set_param(synth, "adsr.attack", 0.005f);
            synth_set_param(synth, "adsr.decay", 0.3f);
            synth_set_param(synth, "adsr.sustain", 0.4f);
            synth_set_param(synth, "adsr.release", 1.0f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Organ")) {
            synth_set_param(synth, "adsr.attack", 0.01f);
            synth_set_param(synth, "adsr.decay", 0.1f);
            synth_set_param(synth, "adsr.sustain", 0.9f);
            synth_set_param(synth, "adsr.release", 0.3f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Brass")) {
            synth_set_param(synth, "adsr.attack", 0.1f);
            synth_set_param(synth, "adsr.decay", 0.2f);
            synth_set_param(synth, "adsr.sustain", 0.8f);
            synth_set_param(synth, "adsr.release", 0.4f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Guitar")) {
            synth_set_param(synth, "adsr.attack", 0.002f);
            synth_set_param(synth, "adsr.decay", 0.4f);
            synth_set_param(synth, "adsr.sustain", 0.6f);
            synth_set_param(synth, "adsr.release", 0.8f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Synth")) {
            synth_set_param(synth, "adsr.attack", 0.02f);
            synth_set_param(synth, "adsr.decay", 0.3f);
            synth_set_param(synth, "adsr.sustain", 0.7f);
            synth_set_param(synth, "adsr.release", 0.5f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Percussion")) {
            synth_set_param(synth, "adsr.attack", 0.001f);
            synth_set_param(synth, "adsr.decay", 0.02f);
            synth_set_param(synth, "adsr.sustain", 0.0f);
            synth_set_param(synth, "adsr.release", 0.05f);
        }
        ImGui::NextColumn();
        
        if (ImGui::Button("Fade In")) {
            synth_set_param(synth, "adsr.attack", 2.0f);
            synth_set_param(synth, "adsr.decay", 0.5f);
            synth_set_param(synth, "adsr.sustain", 0.8f);
            synth_set_param(synth, "adsr.release", 3.0f);
        }
        
        ImGui::Columns(1, "", false);
    }

    // LFOs
    if (ImGui::CollapsingHeader("LFOs", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(3, "lfo_columns", true);
        
        const char* lfo_names[] = {"Pitch", "Volume", "Filter"};
        const char* waveforms[] = {"SINE", "TRIANGLE", "SQUARE", "SAW", "RANDOM"};
        const char* sync_modes[] = {"Free", "Retrigger", "Keyfollow"};
        
        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(i);
            ImGui::BeginChild(lfo_names[i], ImVec2(0, 280), true, 0);
            ImGui::Text("%s", lfo_names[i]);
            
            // Enable/Disable toggle button
            if (synth->lfos[i].enabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
                if (ImGui::Button("ON")) {
                    synth->lfos[i].enabled = 0;
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                if (ImGui::Button("OFF")) {
                    synth->lfos[i].enabled = 1;
                }
                ImGui::PopStyleColor();
            }
            
            // Waveform selection
            int current_waveform = (int)synth->lfos[i].waveform;
            if (ImGui::Combo("Waveform", &current_waveform, waveforms, IM_ARRAYSIZE(waveforms))) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.waveform", i + 1);
                synth_set_param(synth, param_name, (float)current_waveform);
            }
            
            // Frequency slider
            if (ImGui::SliderFloat("Frequency (Hz)", &synth->lfos[i].frequency, 0.1f, 20.0f, "%.2f", 0)) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.frequency", i + 1);
                synth_set_param(synth, param_name, synth->lfos[i].frequency);
            }
            
            // Depth slider
            if (ImGui::SliderFloat("Depth", &synth->lfos[i].depth, 0.0f, 1.0f, "%.2f", 0)) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.depth", i + 1);
                synth_set_param(synth, param_name, synth->lfos[i].depth);
            }
            
            // Phase slider
            if (ImGui::SliderFloat("Phase", &synth->lfos[i].phase, 0.0f, 1.0f, "%.2f", 0)) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.phase", i + 1);
                synth_set_param(synth, param_name, synth->lfos[i].phase);
            }
            
            // Sync mode selection
            int current_sync = (int)synth->lfos[i].sync;
            if (ImGui::Combo("Sync", &current_sync, sync_modes, IM_ARRAYSIZE(sync_modes))) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.sync", i + 1);
                synth_set_param(synth, param_name, (float)current_sync);
            }
            
            ImGui::EndChild();
            ImGui::PopID();
            ImGui::NextColumn();
        }
        ImGui::Columns(1, "", false);
    }

    // Effects
    if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "fx_columns", true);
        ImGui::SetColumnWidth(0, (float)window_width / 2.0f);
        ImGui::SetColumnWidth(1, (float)window_width / 2.0f);

        // Analog Filter
        ImGui::Text("Analog Filter");
        ImGui::Checkbox("Enabled##filter", (bool*)&synth->fx.filter_enabled);
        
        const char* filter_types[] = { "Lowpass", "Highpass", "Bandpass", "Notch" };
        int current_filter_type = 0; // Default to lowpass
        if (ImGui::Combo("Type##filter", &current_filter_type, filter_types, IM_ARRAYSIZE(filter_types))) {
            analog_filter_set_param(&synth->fx.filter, "type", (float)current_filter_type);
        }
        
        ImGui::SliderFloat("Cutoff (Hz)##filter", &synth->fx.filter_cutoff, 20.0f, 20000.0f, "%.1f", 
                          ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Resonance##filter", &synth->fx.filter_resonance, 0.1f, 10.0f, "%.2f", 0);
        ImGui::SliderFloat("Drive##filter", &synth->fx.filter_drive, 1.0f, 10.0f, "%.2f", 0);
        ImGui::SliderFloat("Mix##filter", &synth->fx.filter_mix, 0.0f, 1.0f, "%.2f", 0);
        
        const char* oversampling_rates[] = { "1x", "2x", "4x", "8x" };
        if (ImGui::Combo("Oversampling##filter", &synth->fx.filter_oversampling, oversampling_rates, IM_ARRAYSIZE(oversampling_rates))) {
            analog_filter_set_param(&synth->fx.filter, "oversampling", (float)synth->fx.filter_oversampling);
        }
        
        ImGui::NextColumn();

        ImGui::Text("Ring Modulator");
        if (ImGui::Checkbox("Enabled##ring_mod", (bool*)&synth->ring_mod.enabled)) {
            synth_set_param(synth, "ring_mod.enabled", (float)synth->ring_mod.enabled);
        }
        if (ImGui::SliderFloat("Frequency (Hz)##ring_mod", &synth->ring_mod.frequency, 0.1f, 5000.0f, "%.1f", ImGuiSliderFlags_Logarithmic)) {
            synth_set_param(synth, "ring_mod.frequency", synth->ring_mod.frequency);
        }
        if (ImGui::SliderFloat("Mix##ring_mod", &synth->ring_mod.mix, 0.0f, 1.0f, "%.2f", 0)) {
            synth_set_param(synth, "ring_mod.mix", synth->ring_mod.mix);
        }

        ImGui::Separator();
        
        ImGui::Text("Flanger");
        ImGui::SliderFloat("Depth##flanger", &synth->fx.flanger_depth, 0.0f, 1.0f, "%.2f", 0);
        ImGui::SliderFloat("Rate##flanger", &synth->fx.flanger_rate, 0.0f, 5.0f, "%.2f", 0);
        ImGui::SliderFloat("Feedback##flanger", &synth->fx.flanger_feedback, 0.0f, 1.0f, "%.2f", 0);

        ImGui::Separator();
        ImGui::Text("Delay");
        static bool delay_enabled = false;
        if (ImGui::Checkbox("Enabled##delay", &delay_enabled)) {
            // Toggle between standard delay and multi-tap
            if (delay_enabled) {
                synth_set_param(synth, "delay.mix", 0.4f);
            } else {
                synth_set_param(synth, "delay.mix", 0.0f);
            }
        }
        
        ImGui::SliderFloat("Time##delay", &synth->fx.delay_time, 0.0f, 1.0f, "%.2f", 0);
        if (ImGui::SliderFloat("Feedback##delay", &synth->fx.delay_feedback, 0.0f, 1.0f, "%.2f", 0)) {
            synth_set_param(synth, "delay.feedback", synth->fx.delay_feedback);
        }
        if (ImGui::SliderFloat("Mix##delay", &synth->fx.delay_mix, 0.0f, 1.0f, "%.2f", 0)) {
            synth_set_param(synth, "delay.mix", synth->fx.delay_mix);
            // Update checkbox state based on mix
            delay_enabled = (synth->fx.delay_mix > 0.0f);
        }

        ImGui::Columns(1, "", false);
        
        ImGui::Separator();
        
        ImGui::Columns(1, "reverb_column", true);
        ImGui::Text("Reverb");
        ImGui::SliderFloat("Size##reverb", &synth->fx.reverb_size, 0.0f, 1.0f, "%.2f", 0);
        ImGui::SliderFloat("Mix##reverb", &synth->fx.reverb_mix, 0.0f, 1.0f, "%.2f", 0);
        ImGui::SliderFloat("Damping##reverb", &synth->fx.reverb_damping, 0.0f, 1.0f, "%.2f", 0);

        ImGui::Columns(1, "", false);
    }

    // Mixer
    if (ImGui::CollapsingHeader("Mixer", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Master volume, pan, and width
        ImGui::Text("Master");
        ImGui::SliderFloat("Volume", &synth->mixer.master, 0.0f, 2.0f, "%.2f", 0);
        ImGui::SliderFloat("Pan", &synth->mixer.master_pan, -1.0f, 1.0f, "%.2f", 0);
        ImGui::SliderFloat("Width", &synth->mixer.master_width, 0.0f, 2.0f, "%.2f", 0);

        ImGui::Separator();
        
        // Compressor section
        ImGui::Text("Compressor");
        ImGui::Checkbox("Enabled##comp", (bool*)&synth->mixer.comp_enabled);
        
        // Always show compressor parameters (disabled controls are grayed out automatically by ImGui)
        ImGui::Columns(2, "comp_params", true);
        ImGui::SetColumnWidth(0, (float)window_width / 3.0f);
        ImGui::SetColumnWidth(1, (float)window_width / 3.0f);
        
        // Left column
        ImGui::Text("Threshold");
        ImGui::SliderFloat("##threshold", &synth->mixer.comp_threshold, -24.0f, 0.0f, "%.1f dB", 0);
        
        ImGui::Text("Ratio");
        ImGui::SliderFloat("##ratio", &synth->mixer.comp_ratio, 1.0f, 10.0f, "%.1f:1", 0);
        
        ImGui::NextColumn();
        
        // Right column
        ImGui::Text("Attack");
        ImGui::SliderFloat("##attack", &synth->mixer.comp_attack, 1.0f, 100.0f, "%.1f ms", 0);
        
        ImGui::Text("Release");
        ImGui::SliderFloat("##release", &synth->mixer.comp_release, 10.0f, 1000.0f, "%.0f ms", 0);
        
        ImGui::Columns(1, "", false);
        
        ImGui::Text("Makeup Gain");
        ImGui::SliderFloat("##makeup", &synth->mixer.comp_makeup_gain, 0.0f, 12.0f, "%.1f dB", 0);
	}

    // Mastering
    if (ImGui::CollapsingHeader("Mastering", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("DC Filter");
        ImGui::Checkbox("Enabled##dc_filter", (bool*)&synth->mixer.dc_filter_enabled);
        ImGui::SliderFloat("Cutoff (Hz)##dc_filter", &synth->mixer.dc_filter_freq, 1.0f, 100.0f, "%.1f Hz", 0);
        
        ImGui::Separator();
        
        ImGui::Text("Soft Clipper");
        ImGui::Checkbox("Enabled##soft_clip", (bool*)&synth->mixer.soft_clip_enabled);
        ImGui::SliderFloat("Threshold (dB)##soft_clip", &synth->mixer.soft_clip_threshold, -20.0f, 0.0f, "%.1f dB", 0);
        ImGui::SliderFloat("Knee Ratio##soft_clip", &synth->mixer.soft_clip_ratio, 1.0f, 10.0f, "%.1f", 0);
        
        ImGui::Separator();
        
        ImGui::Text("Auto Gain");
        ImGui::Checkbox("Enabled##auto_gain", (bool*)&synth->mixer.auto_gain_enabled);
        ImGui::SliderFloat("Target (dB)##auto_gain", &synth->mixer.auto_gain_target, -20.0f, 0.0f, "%.1f dB", 0);
        
        ImGui::Separator();
        
        ImGui::Text("Level Meters");
        
        // Create bar graph visualization for levels
        float window_width = ImGui::GetContentRegionAvail().x;
        float bar_width = window_width * 0.4f;
        float bar_height = 20.0f;
        float bar_spacing = 10.0f;
        
        // Peak meter
        ImVec2 peak_bar_pos = ImGui::GetCursorScreenPos();
        float normalized_peak = fmaxf(0.0f, fminf(1.0f, (synth->mixer.peak_level + 60.0f) / 60.0f)); // Normalize -60 to 0 dB
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(peak_bar_pos, 
                              ImVec2(peak_bar_pos.x + bar_width * normalized_peak, peak_bar_pos.y + bar_height),
                              ImColor(0.2f, 0.8f, 0.2f, 1.0f)); // Green bar
        draw_list->AddRect(peak_bar_pos, 
                          ImVec2(peak_bar_pos.x + bar_width, peak_bar_pos.y + bar_height),
                          ImColor(1.0f, 1.0f, 1.0f, 1.0f)); // White border
        // ImGui::Dummy(ImVec2(bar_width + bar_spacing, bar_height));
        ImGui::Text("Peak: %.1f dB", synth->mixer.peak_level);
        
        // RMS meter
        ImVec2 rms_bar_pos = ImGui::GetCursorScreenPos();
        float normalized_rms = fmaxf(0.0f, fminf(1.0f, (synth->mixer.rms_level + 60.0f) / 60.0f)); // Normalize -60 to 0 dB
        draw_list->AddRectFilled(rms_bar_pos, 
                              ImVec2(rms_bar_pos.x + bar_width * normalized_rms, rms_bar_pos.y + bar_height),
                              ImColor(0.8f, 0.2f, 0.2f, 1.0f)); // Orange bar
        draw_list->AddRect(rms_bar_pos, 
                          ImVec2(rms_bar_pos.x + bar_width, rms_bar_pos.y + bar_height),
                          ImColor(1.0f, 1.0f, 1.0f, 1.0f)); // White border
        ImGui::Text("RMS: %.1f dB", synth->mixer.rms_level);

		ImGui::Separator();
	}
        
    // Arpeggiator
    if (ImGui::CollapsingHeader("Arpeggiator", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled##arp", (bool*)&synth->arp.enabled);
        const char* arp_modes[] = { 
            "OFF", "CHORD", "UP", "DOWN", "UP/DOWN", "PENDULUM", 
            "CONVERGE", "DIVERGE", "LEAPFROG", "THUMB-UP", "THUMB-DOWN", 
            "PINKY-UP", "PINKY-DOWN", "REPEAT", "RANDOM", "RANDOM WALK", "SHUFFLE", "ORDER" 
        };
        int current_arp_mode = (int)synth->arp.mode;
        if (ImGui::Combo("Mode", &current_arp_mode, arp_modes, 18)) {
            synth->arp.mode = (ArpMode)current_arp_mode;
        }
        ImGui::SliderFloat("Tempo", &synth->arp.tempo, 30.0f, 240.0f, "%.2f", 0);
        const char* arp_rates[] = { "1/4", "1/8", "1/16", "1/32" };
        int current_arp_rate = (int)synth->arp.rate;
        if (ImGui::Combo("Rate", &current_arp_rate, arp_rates, 4)) {
            synth->arp.rate = (ArpRate)current_arp_rate;
        }
        ImGui::SliderInt("Octave", &synth->arp.octave, 0, 4, "%d");
        ImGui::SliderInt("Octaves", &synth->arp.octaves, 1, 6, "%d");
        ImGui::Checkbox("Polyphonic", (bool*)&synth->arp.polyphonic);
        ImGui::Checkbox("Hold", (bool*)&synth->arp.hold);
        
        ImGui::Separator();
        ImGui::Text("Chord Generation:");
        
        // Chord Type
        const char* chord_types[] = {"MAJOR", "MINOR", "SUS", "DIM"};
        int current_chord_type = (int)synth->arp.chord_type;
        if (ImGui::Combo("Chord Type", &current_chord_type, chord_types, 4)) {
            synth->arp.chord_type = (ChordType)current_chord_type;
        }
        
        // Chord Extensions
        ImGui::Checkbox("Add 6th", (bool*)&synth->arp.add_6);
        ImGui::Checkbox("Add m7", (bool*)&synth->arp.add_m7);
        ImGui::Checkbox("Add Maj7", (bool*)&synth->arp.add_M7);
        ImGui::Checkbox("Add 9th", (bool*)&synth->arp.add_9);
        
        // Voicing
        ImGui::SliderInt("Voicing", &synth->arp.voicing, 0, 16, "%d");
        if (ImGui::IsItemHovered()) {
            const char* voicing_descriptions[] = {
                "Root Position",
                "1st Inversion", "2nd Inversion", "3rd Inversion", "4th Inversion",
                "5th Inversion", "6th Inversion", "7th Inversion", "8th Inversion",
                "Drop 2", "Drop 3", "Drop 2&4", "Drop 2&3",
                "Open Harmony", "Wide Spread", "Clustered", "Alternating Octaves"
            };
            if (synth->arp.voicing >= 0 && synth->arp.voicing <= 16) {
                ImGui::SetTooltip("%s", voicing_descriptions[synth->arp.voicing]);
            }
        }
        
        ImGui::Separator();
        ImGui::Text("Gate & Timing:");
        ImGui::SliderFloat("Gate Length", &synth->arp.gate_length, 0.1f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls note duration (0.1=10%%, 0.5=50%%, 1.0=100%% of step time)");
        }
        
        ImGui::Separator();
        ImGui::Text("Presets:");
        ImGui::Columns(4, "arp_presets", true);
        
        if (ImGui::Button("Classic Up")) {
            synth->arp.mode = ARP_UP;
            synth->arp.tempo = 120.0f;
            synth->arp.octaves = 2;
            synth->arp.polyphonic = 0;
            synth->arp.gate_length = 0.8f;
        }
        ImGui::SameLine(); ImGui::Text("8th notes");
        
        if (ImGui::Button("Classic Down")) {
            synth->arp.mode = ARP_DOWN;
            synth->arp.tempo = 120.0f;
            synth->arp.octaves = 2;
            synth->arp.polyphonic = 0;
            synth->arp.gate_length = 0.8f;
        }
        ImGui::SameLine(); ImGui::Text("Descending");
        
        if (ImGui::Button("Synth Pop")) {
            synth->arp.mode = ARP_UP;
            synth->arp.tempo = 130.0f;
            synth->arp.octaves = 3;
            synth->arp.polyphonic = 1;
            synth->arp.gate_length = 0.7f;
        }
        ImGui::SameLine(); ImGui::Text("Pop style");
        
        if (ImGui::Button("Bass Line")) {
            synth->arp.mode = ARP_ORDER;
            synth->arp.tempo = 128.0f;
            synth->arp.octaves = 1;
            synth->arp.polyphonic = 0;
            synth->arp.gate_length = 0.9f;
        }
        ImGui::SameLine(); ImGui::Text("Sequential");
        
        ImGui::NextColumn();
        
        if (ImGui::Button("Trance Gate")) {
            synth->arp.mode = ARP_UP;
            synth->arp.tempo = 140.0f;
            synth->arp.rate = RATE_SIXTEENTH;
            synth->arp.octaves = 4;
            synth->arp.polyphonic = 1;
            synth->arp.gate_length = 0.4f;
        }
        ImGui::SameLine(); ImGui::Text("16th notes");
        
        if (ImGui::Button("Random Chaos")) {
            synth->arp.mode = ARP_RANDOM;
            synth->arp.tempo = 100.0f;
            synth->arp.octaves = 3;
            synth->arp.polyphonic = 0;
            synth->arp.gate_length = 0.6f;
        }
        ImGui::SameLine(); ImGui::Text("Unpredictable");
        
        if (ImGui::Button("House Chord")) {
            synth->arp.mode = ARP_ORDER;
            synth->arp.tempo = 124.0f;
            synth->arp.octaves = 2;
            synth->arp.polyphonic = 1;
            synth->arp.gate_length = 0.8f;
        }
        ImGui::SameLine(); ImGui::Text("Progressive");
        
        if (ImGui::Button("Sustained Chord")) {
            synth->arp.mode = ARP_CHORD;
            synth->arp.tempo = 60.0f;
            synth->arp.octaves = 1;
            synth->arp.polyphonic = 1;
            synth->arp.gate_length = 1.0f;
        }
        ImGui::SameLine(); ImGui::Text("Chordal");
        
        ImGui::NextColumn();
        
        if (ImGui::Button("Techno Pulse")) {
            synth->arp.mode = ARP_DOWN;
            synth->arp.tempo = 135.0f;
            synth->arp.octaves = 2;
            synth->arp.polyphonic = 0;
            synth->arp.gate_length = 0.3f;
        }
        ImGui::SameLine(); ImGui::Text("Driving");
        
        if (ImGui::Button("Dub Sequence")) {
            synth->arp.mode = ARP_UP;
            synth->arp.tempo = 70.0f;
            synth->arp.octaves = 1;
            synth->arp.polyphonic = 0;
            synth->arp.gate_length = 0.7f;
        }
        ImGui::SameLine(); ImGui::Text("Reggae");
        
        if (ImGui::Button("DnB Roll")) {
            synth->arp.mode = ARP_RANDOM;
            synth->arp.tempo = 174.0f;
            synth->arp.rate = RATE_THIRTYSECOND;
            synth->arp.octaves = 2;
            synth->arp.polyphonic = 1;
            synth->arp.gate_length = 0.2f;
        }
        ImGui::SameLine(); ImGui::Text("Fast");
        
        if (ImGui::Button("Video Game")) {
            synth->arp.mode = ARP_UP;
            synth->arp.tempo = 110.0f;
            synth->arp.octaves = 2;
            synth->arp.polyphonic = 0;
            synth->arp.gate_length = 0.5f;
        }
        ImGui::SameLine(); ImGui::Text("8-bit");
        
        ImGui::NextColumn();
        
        if (ImGui::Button("Classic Arp")) {
            synth->arp.mode = ARP_UP;
            synth->arp.tempo = 92.0f;
            synth->arp.octaves = 4;
            synth->arp.polyphonic = 0;
            synth->arp.gate_length = 0.8f;
        }
        ImGui::SameLine(); ImGui::Text("Classic");
        
        if (ImGui::Button("Dream Pop")) {
            synth->arp.mode = ARP_ORDER;
            synth->arp.tempo = 80.0f;
            synth->arp.octaves = 3;
            synth->arp.polyphonic = 1;
            synth->arp.gate_length = 0.9f;
        }
        ImGui::SameLine(); ImGui::Text("Ethereal");
        
        if (ImGui::Button("Industrial")) {
            synth->arp.mode = ARP_RANDOM;
            synth->arp.tempo = 160.0f;
            synth->arp.gate_length = 0.2f; // Staccato industrial feel
            synth->arp.octaves = 2;
            synth->arp.polyphonic = 0;
        }
        ImGui::SameLine(); ImGui::Text("Harsh");
        
        if (ImGui::Button("Pendulum")) {
            synth->arp.mode = ARP_PENDULUM;
            synth->arp.tempo = 120.0f;
            synth->arp.octaves = 2;
            synth->arp.polyphonic = 0;
            synth->arp.gate_length = 0.6f; // Medium gate for swing feel
        }
        ImGui::SameLine(); ImGui::Text("Bouncing");
        
        ImGui::Columns(1, "", false);
    }

    // Pattern / Melody
    if (ImGui::CollapsingHeader("Pattern / Melody", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "pattern_columns", true);
        
        ImGui::Text("Chord Progression");
        if (g_toggle_chord_progression && g_chord_prog_enabled) {
            const char* label = *g_chord_prog_enabled ? "Stop Progression" : "Start Progression";
            if (ImGui::Button(label, ImVec2(160, 0))) {
                g_toggle_chord_progression();
            }
        }
        
        ImGui::Text("Playback Speed");
        if (g_bpm) {
            ImGui::SliderFloat("BPM", g_bpm, 60.0f, 200.0f, "%.1f");
        }
        
        ImGui::Text("Multi-Tap Delay");
        if (g_synth) {
            // Sync checkbox with actual state
            bool multitap_enabled = g_synth->fx.multitap_enabled;
            if (ImGui::Checkbox("Enable", &multitap_enabled)) {
                fx_set_param(&g_synth->fx, "multitap.enabled", multitap_enabled ? 1.0f : 0.0f);
                // When enabling multi-tap, also ensure delay mix is set
                if (multitap_enabled && g_synth->fx.delay_mix <= 0.0f) {
                    fx_set_param(&g_synth->fx, "delay.mix", 0.5f);
                }
            }
            
            // Use actual values from FX struct, not static variables
            float tap0_beat = g_synth->fx.multitap_taps[0];
            float tap0_level = g_synth->fx.multitap_levels[0];
            float tap1_beat = g_synth->fx.multitap_taps[1];
            float tap1_level = g_synth->fx.multitap_levels[1];
            float tap2_beat = g_synth->fx.multitap_taps[2];
            float tap2_level = g_synth->fx.multitap_levels[2];
            float tap3_beat = g_synth->fx.multitap_taps[3];
            float tap3_level = g_synth->fx.multitap_levels[3];
            
            ImGui::Text("Tap 1 (Quarter):");
            if (ImGui::SliderFloat("##tap0_beat", &tap0_beat, 0.125f, 2.0f, "%.3f beats")) {
                fx_set_param(&g_synth->fx, "multitap.tap0", tap0_beat);
            }
            ImGui::SameLine();
            if (ImGui::SliderFloat("##tap0_level", &tap0_level, 0.0f, 1.0f, "%.2f")) {
                fx_set_param(&g_synth->fx, "multitap.tap0_level", tap0_level);
            }
            
            ImGui::Text("Tap 2 (Dotted 8th):");
            if (ImGui::SliderFloat("##tap1_beat", &tap1_beat, 0.125f, 2.0f, "%.3f beats")) {
                fx_set_param(&g_synth->fx, "multitap.tap1", tap1_beat);
            }
            ImGui::SameLine();
            if (ImGui::SliderFloat("##tap1_level", &tap1_level, 0.0f, 1.0f, "%.2f")) {
                fx_set_param(&g_synth->fx, "multitap.tap1_level", tap1_level);
            }
            
            ImGui::Text("Tap 3 (Eighth):");
            if (ImGui::SliderFloat("##tap2_beat", &tap2_beat, 0.125f, 2.0f, "%.3f beats")) {
                fx_set_param(&g_synth->fx, "multitap.tap2", tap2_beat);
            }
            ImGui::SameLine();
            if (ImGui::SliderFloat("##tap2_level", &tap2_level, 0.0f, 1.0f, "%.2f")) {
                fx_set_param(&g_synth->fx, "multitap.tap2_level", tap2_level);
            }
            
            ImGui::Text("Tap 4 (Triplet):");
            if (ImGui::SliderFloat("##tap3_beat", &tap3_beat, 0.125f, 2.0f, "%.3f beats")) {
                fx_set_param(&g_synth->fx, "multitap.tap3", tap3_beat);
            }
            ImGui::SameLine();
            if (ImGui::SliderFloat("##tap3_level", &tap3_level, 0.0f, 1.0f, "%.2f")) {
                fx_set_param(&g_synth->fx, "multitap.tap3_level", tap3_level);
            }
        }
        
        ImGui::Text("Humanize");
        if (g_velocity_var && g_timing_var) {
            ImGui::SliderFloat("Velocity Var", g_velocity_var, 0.0f, 0.5f, "%.2f");
            ImGui::SliderFloat("Timing Var", g_timing_var, 0.0f, 0.1f, "%.3f");
        }
        
        ImGui::NextColumn();
        
        ImGui::Text("Status");
        if (chord_prog_enabled) {
            ImGui::Text("Playing: Yes");
            ImGui::Text("Chord: %d/%d", current_chord_idx + 1, chord_prog_len);
            ImGui::Text("Step: %d/%d", current_rhythm_step + 1, rhythm_len);
        } else {
            ImGui::Text("Playing: No");
            ImGui::Text("Press F1 to start");
        }
        
        ImGui::Columns(1, "", false);
    }

    ImGui::End();

    // Rendering
    ImGui::Render();

    SDL_GL_MakeCurrent(window, gl_context);
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
