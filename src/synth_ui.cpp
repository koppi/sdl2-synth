#include "synth_ui.h"
#include "imgui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Compact rotary knob widget with built-in MIDI Learn support.
//
// Left-drag vertically (hold Shift for fine control) or hover + scroll wheel
// to change the value. While the global MIDI Learn toggle is on, right-click
// a knob to arm it, then move a hardware MIDI CC controller to bind that
// controller to this exact parameter. The binding writes straight into the
// same memory the knob displays, so applying a learned mapping later needs
// no extra bookkeeping - the knob just reflects the new value next frame.
// ---------------------------------------------------------------------------
static const float KNOB_SM = 32.0f;
static const float KNOB_PI = 3.14159265358979323846f;

// MIDI state of the draw call in progress (null when MIDI Learn is unavailable).
// The UI is only ever drawn from one thread at a time.
static Midi *s_midi = nullptr;

static bool KnobRaw(const char *label, float *display, float v_min, float v_max,
                     const char *format, float diameter, bool logarithmic,
                     void *midi_value, int midi_is_int) {
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    ImGui::PushID(label);
    ImGui::PushID(midi_value);
    ImGui::BeginGroup();

    ImGui::TextUnformatted(label);

    float radius = diameter * 0.5f;
    ImVec2 knob_pos = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(knob_pos.x + radius, knob_pos.y + radius);

    ImGui::InvisibleButton("##k", ImVec2(diameter, diameter));
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();
    bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    bool changed = false;

    float range = v_max - v_min;
    float t = logarithmic ? logf(*display / v_min) / logf(v_max / v_min)
                           : (*display - v_min) / range;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

    if (active && io.MouseDelta.y != 0.0f) {
        t -= io.MouseDelta.y * (io.KeyShift ? 0.0015f : 0.006f);
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        *display = logarithmic ? v_min * powf(v_max / v_min, t) : v_min + t * range;
        changed = true;
    }
    if (hovered && io.MouseWheel != 0.0f) {
        t += io.MouseWheel * 0.02f;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        *display = logarithmic ? v_min * powf(v_max / v_min, t) : v_min + t * range;
        changed = true;
    }

    Midi *midi = s_midi;
    bool armed = midi && midi_is_armed(midi, midi_value);
    const MidiLearnedMapping *mapping = midi ? midi_find_mapping(midi, midi_value) : NULL;

    if (right_clicked && midi && midi->learn_mode) {
        midi_arm_learn(midi, label, midi_value, midi_is_int, v_min, v_max);
        armed = true;
        mapping = NULL;
    }

    ImU32 col_track = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 col_fill = ImGui::GetColorU32(active ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
    ImU32 col_border = ImGui::GetColorU32(ImGuiCol_Border);

    if (armed) {
        float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 6.0f);
        draw_list->AddCircle(center, radius + 3.0f,
                              ImGui::GetColorU32(ImVec4(1.0f, 0.55f + 0.25f * pulse, 0.1f, 1.0f)), 20, 2.5f);
    } else if (mapping) {
        draw_list->AddCircle(center, radius + 3.0f, IM_COL32(80, 220, 120, 255), 20, 1.5f);
    }

    draw_list->AddCircleFilled(center, radius, col_track, 20);
    draw_list->AddCircle(center, radius, col_border, 20, 1.0f);

    const float angle_min = 0.75f * KNOB_PI;
    const float angle_max = 2.25f * KNOB_PI;
    float angle = angle_min + t * (angle_max - angle_min);

    if (t > 0.001f) {
        draw_list->PathArcTo(center, radius - 2.5f, angle_min, angle, 18);
        draw_list->PathStroke(col_fill, 0, 2.5f);
    }

    ImVec2 dir = ImVec2(cosf(angle), sinf(angle));
    draw_list->AddLine(ImVec2(center.x + dir.x * radius * 0.28f, center.y + dir.y * radius * 0.28f),
                        ImVec2(center.x + dir.x * radius * 0.85f, center.y + dir.y * radius * 0.85f),
                        col_border, 2.0f);

    if (hovered || active) {
        char valtxt[64];
        snprintf(valtxt, sizeof(valtxt), format, *display);
        if (mapping)
            ImGui::SetTooltip("%s: %s\nMIDI CC %d", label, valtxt, mapping->cc);
        else if (armed)
            ImGui::SetTooltip("%s: %s\nMove a MIDI CC controller to map...", label, valtxt);
        else
            ImGui::SetTooltip("%s: %s", label, valtxt);
    }

    ImGui::EndGroup();
    ImGui::PopID();
    ImGui::PopID();
    return changed;
}

static bool Knob(const char *label, float *value, float v_min, float v_max,
                  const char *format = "%.2f", float diameter = KNOB_SM, bool logarithmic = false) {
    return KnobRaw(label, value, v_min, v_max, format, diameter, logarithmic, value, 0);
}

static bool KnobInt(const char *label, int *value, int v_min, int v_max,
                     const char *format = "%.0f", float diameter = KNOB_SM) {
    float f = (float)*value;
    bool changed = KnobRaw(label, &f, (float)v_min, (float)v_max, format, diameter, false, value, 1);
    if (changed) {
        *value = (int)(f + (f >= 0.0f ? 0.5f : -0.5f));
    }
    return changed;
}

// Checkbox bound to an int used as a boolean flag.
static bool CheckboxInt(const char *label, int *value) {
    bool b = *value != 0;
    bool changed = ImGui::Checkbox(label, &b);
    if (changed)
        *value = b ? 1 : 0;
    return changed;
}

// ---------------------------------------------------------------------------
// Oscilloscope: a stereo-looking pair of triggered, auto-scaled waveform views
// drawn with ImGui draw lists from the audio thread's ScopeBuffer.
// ---------------------------------------------------------------------------
static void DrawScope(const ScopeBuffer *scope, float width, float height) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const int N = SCOPE_SIZE;

    const float channel_width = floorf(width / 2.0f);
    const float gap = 2.0f;
    const float xs[2] = {origin.x, origin.x + channel_width + gap};
    const ImU32 wave_colors[2] = {IM_COL32(255, 100, 100, 255), IM_COL32(100, 255, 100, 255)};
    const char *labels[2] = {"L", "R"};

    for (int ch = 0; ch < 2; ++ch) {
        const float x0 = xs[ch];
        const float y0 = origin.y;
        const float x1 = x0 + channel_width;
        const float y1 = y0 + height;

        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(20, 25, 35, 255));

        // Grid
        for (int i = 1; i < 8; i++) {
            float gx = x0 + floorf(channel_width * i / 8.0f);
            dl->AddLine(ImVec2(gx, y0), ImVec2(gx, y1), IM_COL32(40, 45, 55, 255));
        }
        for (int i = 1; i < 4; i++) {
            float gy = y0 + floorf(height * i / 4.0f);
            dl->AddLine(ImVec2(x0, gy), ImVec2(x1, gy), IM_COL32(40, 45, 55, 255));
        }
        // Centre line (0 V reference)
        dl->AddLine(ImVec2(x0, y0 + floorf(height / 2.0f)), ImVec2(x1, y0 + floorf(height / 2.0f)),
                    IM_COL32(80, 85, 100, 255));
    }

    if (scope && scope->ready && channel_width > 2.0f) {
        const float *buf = scope->samples;
        const int samples_to_display = (int)channel_width > N ? N : (int)channel_width;

        // Look backwards from the write position for a rising zero crossing so
        // the trace stays put on periodic signals.
        int trigger_pos = scope->write_pos % N;
        const int search_range = N / 4;
        int stable_trigger = -1;
        int best_trigger = trigger_pos;
        float min_crossing_diff = 1.0f;

        for (int offset = 0; offset < search_range; offset++) {
            int idx1 = (trigger_pos - offset - 1 + N) % N;
            int idx2 = (trigger_pos - offset + N) % N;
            float sample1 = buf[idx1];
            float sample2 = buf[idx2];

            if (sample1 < 0.0f && sample2 >= 0.0f) {
                float crossing_diff = fabsf(sample1) + fabsf(sample2);
                if (crossing_diff < min_crossing_diff) {
                    min_crossing_diff = crossing_diff;
                    best_trigger = idx2;

                    // Make sure this isn't just noise around zero.
                    int stable_count = 0;
                    float amplitude_sum = 0.0f;
                    for (int check = 0; check < 8 && check < samples_to_display; check++) {
                        float check_sample = buf[(idx2 + check) % N];
                        amplitude_sum += fabsf(check_sample);
                        if (fabsf(check_sample) > 0.01f)
                            stable_count++;
                    }
                    if (stable_count >= 4 && amplitude_sum / 8.0f > 0.02f)
                        stable_trigger = idx2;
                }
            }
        }
        if (stable_trigger >= 0)
            trigger_pos = stable_trigger;
        else if (best_trigger != trigger_pos)
            trigger_pos = best_trigger;

        // Auto-scale to the visible signal range.
        float min_sample = buf[trigger_pos % N], max_sample = min_sample;
        for (int i = 0; i < samples_to_display; i++) {
            float s = buf[(trigger_pos + i) % N];
            if (s < min_sample) min_sample = s;
            if (s > max_sample) max_sample = s;
        }
        float signal_range = max_sample - min_sample;
        float scale_factor;
        if (signal_range < 0.001f)
            scale_factor = (height / 2.0f) * 0.8f;
        else
            scale_factor = (height / 2.0f) * 0.9f / (signal_range / 2.0f);
        float signal_center = (max_sample + min_sample) / 2.0f;
        float center_y = origin.y + floorf(height / 2.0f);

        const int px_count = (int)channel_width;
        static ImVector<ImVec2> points; // reused every frame to avoid reallocation
        points.resize(px_count);
        for (int ch = 0; ch < 2; ++ch) {
            for (int px = 0; px < px_count; px++) {
                int idx = (trigger_pos + (px * samples_to_display) / px_count) % N;
                float y = center_y - (buf[idx] - signal_center) * scale_factor;
                if (y < origin.y) y = origin.y;
                if (y > origin.y + height - 1.0f) y = origin.y + height - 1.0f;
                points[px] = ImVec2(xs[ch] + (float)px, y);
            }
            dl->AddPolyline(points.Data, px_count, wave_colors[ch], 0, 1.0f);
        }
    }

    for (int ch = 0; ch < 2; ++ch) {
        dl->AddRect(ImVec2(xs[ch], origin.y), ImVec2(xs[ch] + channel_width, origin.y + height),
                    IM_COL32(100, 100, 120, 255));
        dl->AddText(ImVec2(xs[ch] + 5.0f, origin.y + 5.0f), IM_COL32(180, 180, 200, 255), labels[ch]);
    }

    ImGui::Dummy(ImVec2(width, height));
}

static void DrawMultiTapDelay(Synth *synth) {
    ImGui::Text("Multi-Tap Delay");
    ImGui::SameLine();
    // Sync checkbox with actual state
    bool multitap_enabled = synth->fx.multitap_enabled;
    if (ImGui::Checkbox("Enable", &multitap_enabled)) {
        fx_set_param(&synth->fx, "multitap.enabled", multitap_enabled ? 1.0f : 0.0f);
        // When enabling multi-tap, also ensure delay mix is set
        if (multitap_enabled && synth->fx.delay_mix <= 0.0f) {
            fx_set_param(&synth->fx, "delay.mix", 0.5f);
        }
    }

    const char* tap_names[4] = {"Tap 1", "Tap 2", "Tap 3", "Tap 4"};
    const char* tap_descs[4] = {"Quarter", "Dotted 8th", "Eighth", "Triplet"};
    const char* tap_params[4] = {"multitap.tap0", "multitap.tap1", "multitap.tap2", "multitap.tap3"};
    const char* level_params[4] = {"multitap.tap0_level", "multitap.tap1_level", "multitap.tap2_level", "multitap.tap3_level"};
    ImGui::Columns(4, "multitap_columns", true);
    for (int tap = 0; tap < 4; ++tap) {
        ImGui::PushID(tap);
        ImGui::TextUnformatted(tap_names[tap]);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tap_descs[tap]);
        }
        if (Knob("Beat", &synth->fx.multitap_taps[tap], 0.125f, 2.0f, "%.3f")) {
            fx_set_param(&synth->fx, tap_params[tap], synth->fx.multitap_taps[tap]);
        }
        ImGui::SameLine();
        if (Knob("Level", &synth->fx.multitap_levels[tap], 0.0f, 1.0f, "%.2f")) {
            fx_set_param(&synth->fx, level_params[tap], synth->fx.multitap_levels[tap]);
        }
        ImGui::PopID();
        ImGui::NextColumn();
    }
    ImGui::Columns(1, "", false);
}

void SynthUi::draw(Synth *synth, float width, float height, const SynthUiOptions &opt) {
    s_midi = opt.midi;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::Begin("Synth", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Tighten spacing throughout so the whole parameter set fits without scrolling.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 2.0f));

    // ---- MIDI Learn toolbar: always visible, spans the full window ----
    if (opt.midi) {
        Midi *midi = opt.midi;
        bool learn_on = midi->learn_mode;
        if (learn_on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.55f, 0.05f, 1.0f));
        if (ImGui::Button(learn_on ? "MIDI Learn: ON" : "MIDI Learn: OFF", ImVec2(150, 0))) {
            midi->learn_mode = !midi->learn_mode;
            midi->learn_armed_value = NULL;
        }
        if (learn_on) ImGui::PopStyleColor();
        ImGui::SameLine();
        if (learn_on) {
            if (midi->learn_armed_value)
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Move a MIDI CC controller to map \"%s\"...", midi->learn_armed_label);
            else
                ImGui::TextDisabled("Right-click any knob to arm it for MIDI mapping.");
        } else if (midi->learned_count > 0) {
            ImGui::TextDisabled("%d MIDI mapping%s active.", midi->learned_count, midi->learned_count == 1 ? "" : "s");
        } else {
            ImGui::TextDisabled("Turn on to map hardware MIDI knobs to on-screen controls.");
        }
        if (midi->learned_count > 0) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear Mappings")) {
                midi_clear_learned(midi);
            }
        }
        if (midi->last_cc >= 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(last CC %d = %d)", midi->last_cc, midi->last_cc_value);
        }
        ImGui::Separator();
    }

    float panel_width = width * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;
    ImGui::BeginChild("LeftPanel", ImVec2(panel_width, 0), false);

    // Oscilloscope
    if (opt.scope && ImGui::CollapsingHeader("Oscilloscope", ImGuiTreeNodeFlags_DefaultOpen)) {
        const float osc_child_height = 160.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::BeginChild("OscilloscopeChild", ImVec2(0, osc_child_height), false, ImGuiWindowFlags_NoScrollbar);
        ImVec2 content_size = ImGui::GetContentRegionAvail();
        if (content_size.x > 0 && content_size.y > 0) {
            DrawScope(opt.scope, content_size.x, osc_child_height);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    // Oscillators
    if (ImGui::CollapsingHeader("Oscillators", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "osc_columns", true);
        for (int i = 0; i < 4; ++i) {
            ImGui::PushID(i);
            char title[32];
            snprintf(title, sizeof(title), "OSC %d", i + 1);

            ImGui::BeginChild(title, ImVec2(0, 118), true, 0);
            ImGui::Text("%s", title);

            int current_item = (int)synth->osc[i].waveform;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##waveform", &current_item, "SINE\0SAW\0SQUARE\0TRI\0NOISE\0\0", 5)) {
                synth->osc[i].waveform = (OscWaveform)current_item;
            }

            Knob("Pitch", &synth->osc[i].pitch, -24.0f, 24.0f, "%.2f");
            ImGui::SameLine();
            Knob("Detune", &synth->osc[i].detune, -1.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            Knob("Gain", &synth->osc[i].gain, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            Knob("Pan", &synth->osc[i].pan, -1.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            Knob("PW", &synth->osc[i].pulse_width, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            KnobInt("Unison", &synth->osc[i].unison_voices, 1, 8);
            ImGui::SameLine();
            Knob("U.Det", &synth->osc[i].unison_detune, 0.0f, 1.0f, "%.2f");

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
        ImGui::Columns(4, "random_buttons", true);

        if (ImGui::Button("OSC 1")) {
            synth_randomize_oscillator(synth, 0);
        }
        ImGui::NextColumn();
        if (ImGui::Button("OSC 2")) {
            synth_randomize_oscillator(synth, 1);
        }
        ImGui::NextColumn();
        if (ImGui::Button("OSC 3")) {
            synth_randomize_oscillator(synth, 2);
        }
        ImGui::NextColumn();
        if (ImGui::Button("OSC 4")) {
            synth_randomize_oscillator(synth, 3);
        }
        ImGui::Columns(1, "", false);
    }

    // ADSR Envelope
    if (ImGui::CollapsingHeader("ADSR Envelope", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (Knob("Attack", &synth->adsr.attack, 0.001f, 5.0f, "%.3f s", KNOB_SM)) {
            synth_set_param(synth, "adsr.attack", synth->adsr.attack);
        }
        ImGui::SameLine();
        if (Knob("Decay", &synth->adsr.decay, 0.001f, 5.0f, "%.3f s", KNOB_SM)) {
            synth_set_param(synth, "adsr.decay", synth->adsr.decay);
        }
        ImGui::SameLine();
        if (Knob("Sustain", &synth->adsr.sustain, 0.0f, 1.0f, "%.2f", KNOB_SM)) {
            synth_set_param(synth, "adsr.sustain", synth->adsr.sustain);
        }
        ImGui::SameLine();
        if (Knob("Release", &synth->adsr.release, 0.001f, 10.0f, "%.3f s", KNOB_SM)) {
            synth_set_param(synth, "adsr.release", synth->adsr.release);
        }
        ImGui::SameLine();

        // Envelope visualization
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImVec2(130, 62);

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

        ImGui::Dummy(canvas_size); // Reserve space for the drawing

        // Presets
        ImGui::Separator();
        struct AdsrPreset {
            const char *name;
            float attack, decay, sustain, release;
        };
        static const AdsrPreset adsr_presets[] = {
            {"Pad",        1.5f,   0.8f,  0.7f, 2.0f},
            {"Pluck",      0.001f, 0.5f,  0.3f, 0.8f},
            {"Lead",       0.05f,  0.2f,  0.8f, 0.3f},
            {"Bass",       0.01f,  0.1f,  0.9f, 0.1f},
            {"Ambient",    3.0f,   1.5f,  0.6f, 4.0f},
            {"Staccato",   0.001f, 0.05f, 0.0f, 0.1f},
            {"Strings",    0.8f,   0.3f,  0.8f, 1.2f},
            {"Piano",      0.005f, 0.3f,  0.4f, 1.0f},
            {"Organ",      0.01f,  0.1f,  0.9f, 0.3f},
            {"Brass",      0.1f,   0.2f,  0.8f, 0.4f},
            {"Guitar",     0.002f, 0.4f,  0.6f, 0.8f},
            {"Synth",      0.02f,  0.3f,  0.7f, 0.5f},
            {"Percussion", 0.001f, 0.02f, 0.0f, 0.05f},
            {"Fade In",    2.0f,   0.5f,  0.8f, 3.0f},
        };
        ImGui::SetNextItemWidth(160);
        if (ImGui::BeginCombo("Preset", adsr_preset_idx_ >= 0 ? adsr_presets[adsr_preset_idx_].name : "Custom")) {
            for (size_t p = 0; p < IM_ARRAYSIZE(adsr_presets); ++p) {
                bool selected = ((int)p == adsr_preset_idx_);
                if (ImGui::Selectable(adsr_presets[p].name, selected)) {
                    adsr_preset_idx_ = (int)p;
                    synth_set_param(synth, "adsr.attack", adsr_presets[p].attack);
                    synth_set_param(synth, "adsr.decay", adsr_presets[p].decay);
                    synth_set_param(synth, "adsr.sustain", adsr_presets[p].sustain);
                    synth_set_param(synth, "adsr.release", adsr_presets[p].release);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    // LFOs
    if (ImGui::CollapsingHeader("LFOs", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(3, "lfo_columns", true);

        const char* lfo_names[] = {"Pitch", "Volume", "Filter"};
        const char* waveforms[] = {"SINE", "TRIANGLE", "SQUARE", "SAW", "RANDOM"};
        const char* sync_modes[] = {"Free", "Retrigger", "Keyfollow"};

        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(i);
            ImGui::BeginChild(lfo_names[i], ImVec2(0, 150), true, 0);
            ImGui::Text("%s", lfo_names[i]);
            ImGui::SameLine();

            // Enable/Disable toggle button
            if (synth->lfos[i].enabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
                if (ImGui::SmallButton("ON")) {
                    synth->lfos[i].enabled = 0;
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                if (ImGui::SmallButton("OFF")) {
                    synth->lfos[i].enabled = 1;
                }
                ImGui::PopStyleColor();
            }

            // Waveform selection
            int current_waveform = (int)synth->lfos[i].waveform;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##waveform", &current_waveform, waveforms, IM_ARRAYSIZE(waveforms))) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.waveform", i + 1);
                synth_set_param(synth, param_name, (float)current_waveform);
            }

            // Sync mode selection
            int current_sync = (int)synth->lfos[i].sync;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##sync", &current_sync, sync_modes, IM_ARRAYSIZE(sync_modes))) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.sync", i + 1);
                synth_set_param(synth, param_name, (float)current_sync);
            }

            // Frequency knob
            if (Knob("Freq", &synth->lfos[i].frequency, 0.1f, 20.0f, "%.2f Hz")) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.frequency", i + 1);
                synth_set_param(synth, param_name, synth->lfos[i].frequency);
            }
            ImGui::SameLine();

            // Depth knob
            if (Knob("Depth", &synth->lfos[i].depth, 0.0f, 1.0f, "%.2f")) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.depth", i + 1);
                synth_set_param(synth, param_name, synth->lfos[i].depth);
            }
            ImGui::SameLine();

            // Phase knob
            if (Knob("Phase", &synth->lfos[i].phase, 0.0f, 1.0f, "%.2f")) {
                char param_name[32];
                snprintf(param_name, sizeof(param_name), "lfo%d.phase", i + 1);
                synth_set_param(synth, param_name, synth->lfos[i].phase);
            }

            ImGui::EndChild();
            ImGui::PopID();
            ImGui::NextColumn();
        }
        ImGui::Columns(1, "", false);
    }

    // Mixer
    if (ImGui::CollapsingHeader("Mixer", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Master volume, pan, and width
        ImGui::Text("Master");
        Knob("Volume", &synth->mixer.master, 0.0f, 2.0f, "%.2f", KNOB_SM);
        ImGui::SameLine();
        Knob("Pan", &synth->mixer.master_pan, -1.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        Knob("Width", &synth->mixer.master_width, 0.0f, 2.0f, "%.2f");

        ImGui::Separator();

        // Compressor section
        ImGui::Text("Compressor");
        ImGui::SameLine();
        CheckboxInt("On##comp", &synth->mixer.comp_enabled);

        Knob("Thresh", &synth->mixer.comp_threshold, -24.0f, 0.0f, "%.1f dB");
        ImGui::SameLine();
        Knob("Ratio", &synth->mixer.comp_ratio, 1.0f, 10.0f, "%.1f:1");
        ImGui::SameLine();
        if (Knob("Attack", &synth->mixer.comp_attack, 1.0f, 100.0f, "%.1f ms")) {
            mixer_set_param(&synth->mixer, "comp.attack", synth->mixer.comp_attack);
        }
        ImGui::SameLine();
        if (Knob("Release", &synth->mixer.comp_release, 10.0f, 1000.0f, "%.0f ms")) {
            mixer_set_param(&synth->mixer, "comp.release", synth->mixer.comp_release);
        }
        ImGui::SameLine();
        Knob("Makeup", &synth->mixer.comp_makeup_gain, 0.0f, 12.0f, "%.1f dB");
    }

    ImGui::EndChild(); // LeftPanel
    ImGui::SameLine();
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), false);

    // Effects
    if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "fx_columns", true);

        // Analog Filter
        ImGui::Text("Analog Filter");
        ImGui::SameLine();
        CheckboxInt("On##filter", &synth->fx.filter_enabled);

        const char* filter_types[] = { "Lowpass", "Highpass", "Bandpass", "Notch" };
        int current_filter_type = (int)synth->fx.filter.type;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##filtertype", &current_filter_type, filter_types, IM_ARRAYSIZE(filter_types))) {
            analog_filter_set_param(&synth->fx.filter, "type", (float)current_filter_type);
        }

        Knob("Cutoff", &synth->fx.filter_cutoff, 20.0f, 20000.0f, "%.0f Hz", KNOB_SM, true);
        ImGui::SameLine();
        Knob("Res", &synth->fx.filter_resonance, 0.1f, 10.0f, "%.2f");
        ImGui::SameLine();
        Knob("Drive", &synth->fx.filter_drive, 1.0f, 10.0f, "%.2f");
        ImGui::SameLine();
        Knob("Mix", &synth->fx.filter_mix, 0.0f, 1.0f, "%.2f");

        const char* oversampling_rates[] = { "1x", "2x", "4x", "8x" };
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("Oversampling", &synth->fx.filter_oversampling, oversampling_rates, IM_ARRAYSIZE(oversampling_rates));

        ImGui::Separator();
        ImGui::Text("Ring Modulator");
        ImGui::SameLine();
        if (CheckboxInt("On##ring_mod", &synth->ring_mod.enabled)) {
            synth_set_param(synth, "ring_mod.enabled", (float)synth->ring_mod.enabled);
        }
        if (Knob("Freq", &synth->ring_mod.frequency, 0.1f, 5000.0f, "%.1f Hz", KNOB_SM, true)) {
            synth_set_param(synth, "ring_mod.frequency", synth->ring_mod.frequency);
        }
        ImGui::SameLine();
        if (Knob("Mix", &synth->ring_mod.mix, 0.0f, 1.0f, "%.2f")) {
            synth_set_param(synth, "ring_mod.mix", synth->ring_mod.mix);
        }

        ImGui::Separator();
        ImGui::Text("Reverb");
        Knob("Size", &synth->fx.reverb_size, 0.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        Knob("Mix", &synth->fx.reverb_mix, 0.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        Knob("Damping", &synth->fx.reverb_damping, 0.0f, 1.0f, "%.2f");

        ImGui::NextColumn();

        ImGui::Text("Flanger");
        Knob("Depth", &synth->fx.flanger_depth, 0.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        Knob("Rate", &synth->fx.flanger_rate, 0.0f, 5.0f, "%.2f");
        ImGui::SameLine();
        Knob("Feedback", &synth->fx.flanger_feedback, 0.0f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::Text("Delay");
        ImGui::SameLine();
        // The delay is "on" whenever its mix is above zero.
        bool delay_enabled = synth->fx.delay_mix > 0.0f;
        if (ImGui::Checkbox("On##delay", &delay_enabled)) {
            synth->fx.delay_mix = delay_enabled ? 0.4f : 0.0f;
        }

        Knob("Time", &synth->fx.delay_time, 0.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        Knob("Feedback", &synth->fx.delay_feedback, 0.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        Knob("Mix", &synth->fx.delay_mix, 0.0f, 1.0f, "%.2f");

        ImGui::Columns(1, "", false);
    }

    // The plug-in has no "Pattern / Melody" generator, so the multi-tap delay
    // gets a section of its own there.
    if (!opt.show_pattern_section && ImGui::CollapsingHeader("Multi-Tap Delay", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawMultiTapDelay(synth);
    }

    // Mastering
    if (ImGui::CollapsingHeader("Mastering", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("DC Filter");
        ImGui::SameLine();
        CheckboxInt("On##dc_filter", &synth->mixer.dc_filter_enabled);
        ImGui::SameLine();
        Knob("Cutoff", &synth->mixer.dc_filter_freq, 1.0f, 100.0f, "%.1f Hz");

        ImGui::Text("Soft Clipper");
        ImGui::SameLine();
        CheckboxInt("On##soft_clip", &synth->mixer.soft_clip_enabled);
        ImGui::SameLine();
        Knob("Thresh", &synth->mixer.soft_clip_threshold, -20.0f, 0.0f, "%.1f dB");
        ImGui::SameLine();
        Knob("Knee", &synth->mixer.soft_clip_ratio, 1.0f, 10.0f, "%.1f");

        ImGui::Text("Auto Gain");
        ImGui::SameLine();
        CheckboxInt("On##auto_gain", &synth->mixer.auto_gain_enabled);
        ImGui::SameLine();
        Knob("Target", &synth->mixer.auto_gain_target, -20.0f, 0.0f, "%.1f dB");

        ImGui::Separator();

        ImGui::Text("Level Meters");

        // Create bar graph visualization for levels
        float meter_width = ImGui::GetContentRegionAvail().x;
        float bar_width = meter_width * 0.5f;
        float bar_height = 14.0f;

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
        ImGui::Dummy(ImVec2(bar_width, bar_height));
        ImGui::SameLine();
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
        ImGui::Dummy(ImVec2(bar_width, bar_height));
        ImGui::SameLine();
        ImGui::Text("RMS: %.1f dB", synth->mixer.rms_level);
    }

    // Arpeggiator
    if (ImGui::CollapsingHeader("Arpeggiator", ImGuiTreeNodeFlags_DefaultOpen)) {
        CheckboxInt("Enabled##arp", &synth->arp.enabled);
        const char* arp_modes[] = {
            "OFF", "CHORD", "UP", "DOWN", "UP/DOWN", "PENDULUM",
            "CONVERGE", "DIVERGE", "LEAPFROG", "THUMB-UP", "THUMB-DOWN",
            "PINKY-UP", "PINKY-DOWN", "REPEAT", "RANDOM", "RANDOM WALK", "SHUFFLE", "ORDER"
        };
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        int current_arp_mode = (int)synth->arp.mode;
        if (ImGui::Combo("Mode", &current_arp_mode, arp_modes, 18)) {
            synth->arp.mode = (ArpMode)current_arp_mode;
        }
        ImGui::SameLine();
        const char* arp_rates[] = { "1/4", "1/8", "1/16", "1/32" };
        ImGui::SetNextItemWidth(70);
        int current_arp_rate = (int)synth->arp.rate;
        if (ImGui::Combo("Rate", &current_arp_rate, arp_rates, 4)) {
            synth->arp.rate = (ArpRate)current_arp_rate;
        }
        if (opt.show_host_sync) {
            ImGui::SameLine();
            CheckboxInt("Host Sync", &synth->host_sync);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Follow the host tempo (arpeggiator and multi-tap delay)");
            }
        }

        Knob("Tempo", &synth->arp.tempo, 30.0f, 240.0f, "%.0f BPM", KNOB_SM);
        ImGui::SameLine();
        KnobInt("Octave", &synth->arp.octave, 0, 4);
        ImGui::SameLine();
        KnobInt("Octaves", &synth->arp.octaves, 1, 6);
        ImGui::SameLine();
        Knob("Gate", &synth->arp.gate_length, 0.1f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls note duration (0.1=10%%, 0.5=50%%, 1.0=100%% of step time)");
        }
        ImGui::SameLine();
        CheckboxInt("Poly", &synth->arp.polyphonic);
        ImGui::SameLine();
        CheckboxInt("Hold", &synth->arp.hold);

        ImGui::Separator();
        ImGui::Text("Chord:");
        ImGui::SameLine();

        // Chord Type
        const char* chord_types[] = {"MAJOR", "MINOR", "SUS", "DIM"};
        ImGui::SetNextItemWidth(90);
        int current_chord_type = (int)synth->arp.chord_type;
        if (ImGui::Combo("##chordtype", &current_chord_type, chord_types, 4)) {
            synth->arp.chord_type = (ChordType)current_chord_type;
        }

        // Chord Extensions
        ImGui::SameLine();
        CheckboxInt("6th", &synth->arp.add_6);
        ImGui::SameLine();
        CheckboxInt("m7", &synth->arp.add_m7);
        ImGui::SameLine();
        CheckboxInt("Maj7", &synth->arp.add_M7);
        ImGui::SameLine();
        CheckboxInt("9th", &synth->arp.add_9);
        ImGui::SameLine();

        // Voicing
        KnobInt("Voicing", &synth->arp.voicing, 0, 16);
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
        ImGui::Text("Presets:");

        struct ArpPreset {
            const char *name;
            const char *desc;
            ArpMode mode;
            float tempo;
            ArpRate rate;
            int octaves;
            int polyphonic;
            float gate_length;
            int has_rate;
        };
        static const ArpPreset arp_presets[] = {
            {"Classic Up",     "8th notes",     ARP_UP,     120.0f, RATE_EIGHTH,       2, 0, 0.8f, 0},
            {"Classic Down",   "Descending",    ARP_DOWN,   120.0f, RATE_EIGHTH,       2, 0, 0.8f, 0},
            {"Synth Pop",      "Pop style",     ARP_UP,     130.0f, RATE_EIGHTH,       3, 1, 0.7f, 0},
            {"Bass Line",      "Sequential",    ARP_ORDER,  128.0f, RATE_EIGHTH,       1, 0, 0.9f, 0},
            {"Trance Gate",    "16th notes",    ARP_UP,     140.0f, RATE_SIXTEENTH,    4, 1, 0.4f, 1},
            {"Random Chaos",   "Unpredictable", ARP_RANDOM, 100.0f, RATE_EIGHTH,       3, 0, 0.6f, 0},
            {"House Chord",    "Progressive",   ARP_ORDER,  124.0f, RATE_EIGHTH,       2, 1, 0.8f, 0},
            {"Sustained Chord","Chordal",       ARP_CHORD,  60.0f,  RATE_EIGHTH,       1, 1, 1.0f, 0},
            {"Techno Pulse",   "Driving",       ARP_DOWN,   135.0f, RATE_EIGHTH,       2, 0, 0.3f, 0},
            {"Dub Sequence",   "Reggae",        ARP_UP,     70.0f,  RATE_EIGHTH,       1, 0, 0.7f, 0},
            {"DnB Roll",       "Fast",          ARP_RANDOM, 174.0f, RATE_THIRTYSECOND, 2, 1, 0.2f, 1},
            {"Video Game",     "8-bit",         ARP_UP,     110.0f, RATE_EIGHTH,       2, 0, 0.5f, 0},
            {"Classic Arp",    "Classic",       ARP_UP,     92.0f,  RATE_EIGHTH,       4, 0, 0.8f, 0},
            {"Dream Pop",      "Ethereal",      ARP_ORDER,  80.0f,  RATE_EIGHTH,       3, 1, 0.9f, 0},
            {"Industrial",     "Harsh",         ARP_RANDOM, 160.0f, RATE_EIGHTH,       2, 0, 0.2f, 0},
            {"Pendulum",       "Bouncing",      ARP_PENDULUM,120.0f,RATE_EIGHTH,       2, 0, 0.6f, 0},
        };

        ImGui::Columns(8, "arp_presets", true);
        for (size_t p = 0; p < IM_ARRAYSIZE(arp_presets); ++p) {
            const ArpPreset &preset = arp_presets[p];
            if (ImGui::Button(preset.name, ImVec2(-1, 0))) {
                synth->arp.mode = preset.mode;
                synth->arp.tempo = preset.tempo;
                if (preset.has_rate) synth->arp.rate = preset.rate;
                synth->arp.octaves = preset.octaves;
                synth->arp.polyphonic = preset.polyphonic;
                synth->arp.gate_length = preset.gate_length;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", preset.desc);
            }
            ImGui::NextColumn();
        }
        ImGui::Columns(1, "", false);
    }

    // Pattern / Melody (standalone only: the chord progression generator lives in the app)
    if (opt.show_pattern_section && ImGui::CollapsingHeader("Pattern / Melody", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (opt.toggle_chord_progression && opt.chord_prog_enabled) {
            const char* label = *opt.chord_prog_enabled ? "Stop Progression" : "Start Progression";
            if (ImGui::Button(label, ImVec2(150, 0))) {
                opt.toggle_chord_progression();
            }
            ImGui::SameLine();
        }

        if (opt.bpm) {
            Knob("BPM", opt.bpm, 60.0f, 200.0f, "%.1f");
            ImGui::SameLine();
        }

        if (opt.chord_prog_enabled && *opt.chord_prog_enabled) {
            ImGui::Text("Chord: %d/%d  Step: %d/%d", opt.current_chord_idx + 1, opt.chord_prog_len, opt.current_rhythm_step + 1, opt.rhythm_len);
        } else {
            ImGui::Text("Not playing - press F1 to start");
        }

        ImGui::Separator();
        DrawMultiTapDelay(synth);

        ImGui::Separator();
        ImGui::Text("Humanize");
        ImGui::SameLine();
        if (opt.velocity_var && opt.timing_var) {
            Knob("Vel Var", opt.velocity_var, 0.0f, 0.5f, "%.2f");
            ImGui::SameLine();
            Knob("Time Var", opt.timing_var, 0.0f, 0.1f, "%.3f");
        }
    }

    ImGui::EndChild(); // RightPanel

    ImGui::PopStyleVar(2);
    ImGui::End();

    s_midi = nullptr;
}
