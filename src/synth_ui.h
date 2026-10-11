#pragma once

// Dear ImGui user interface of the synth, shared by the standalone SDL2 app
// (gui.cpp) and the VST3 editor (src/vst3/). It only issues ImGui calls, so it
// works with any platform/renderer backend.

#include "synth.h"

struct SynthUiOptions {
  // Hardware MIDI state. When set, the MIDI Learn toolbar is shown and knobs
  // can be armed for learning. The plug-in leaves this null (the host owns MIDI
  // mapping and automation).
  Midi *midi = nullptr;

  // Waveform tap shown in the oscilloscope. Section is hidden when null.
  const ScopeBuffer *scope = nullptr;

  // Standalone-only "Pattern / Melody" section (chord progression generator).
  bool show_pattern_section = false;
  int *chord_prog_enabled = nullptr;
  float *velocity_var = nullptr;
  float *timing_var = nullptr;
  float *bpm = nullptr;
  void (*toggle_chord_progression)(void) = nullptr;
  int current_chord_idx = 0;
  int chord_prog_len = 0;
  int current_rhythm_step = 0;
  int rhythm_len = 0;

  // Plug-in only: show the "Sync to host tempo" option in the arpeggiator.
  bool show_host_sync = false;
};

class SynthUi {
public:
  // Draws the whole UI into the current ImGui frame, filling a window of
  // `width` x `height` at the origin. `synth` is edited in place.
  void draw(Synth *synth, float width, float height, const SynthUiOptions &options);

private:
  int adsr_preset_idx_ = -1;
};
