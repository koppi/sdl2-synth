#pragma once

// Platform independent part of the editor: an ImGui context running the
// shared synth UI on a private "shadow" Synth, a software renderer, and the
// glue that mirrors parameter changes between that model and the host (via the
// edit controller) in both directions.

#include "params.h"
#include "sw_renderer.h"
#include "synth_ui.h"
#include "viz.h"

#include "pluginterfaces/base/ftypes.h"

#include <memory>
#include <vector>

struct ImGuiContext;

namespace sdl2synth {

class SynthController;

class EditorCore {
public:
  explicit EditorCore(SynthController* controller);
  ~EditorCore();

  // Size of the UI in ImGui units and the number of pixels per unit.
  void setLogicalSize(int width, int height);
  void setScale(float scale);
  int logicalWidth() const { return logicalW_; }
  int logicalHeight() const { return logicalH_; }
  int pixelWidth() const { return pixelW_; }
  int pixelHeight() const { return pixelH_; }

  // Input in framebuffer pixels.
  void mouseMove(float x, float y);
  void mouseButton(int button, bool down);
  void mouseWheel(float dx, float dy);
  void mouseLeave();
  void shift(bool down);

  // Runs one frame. Returns the framebuffer regions that changed and need
  // presenting (empty when nothing did). `forceAll` repaints everything.
  const std::vector<PixelRect>& frame(bool forceAll = false);
  const uint32_t* pixels() const { return framebuffer_.data(); }

  // Closes any parameter edit gestures that are still open.
  void finishEdits();

private:
  struct ParamState {
    float plain = 0.0f;
    double normalized = -1.0; // last value known to both sides; <0 = unknown
    bool inGesture = false;
    double lastEditTime = 0.0;
  };

  void resizeFramebuffer();
  void syncFromHost();
  void syncToHost(double now);
  void reportEdit(size_t index, float plain, double now);

  SynthController* controller_;
  std::shared_ptr<VizData> viz_;
  ImGuiContext* context_ = nullptr;
  SynthUi ui_;
  SynthUiOptions options_;
  SwRenderer renderer_;
  std::unique_ptr<Synth> model_;
  std::vector<ParamState> state_;

  int logicalW_ = 1280, logicalH_ = 860;
  float scale_ = 1.0f;
  int pixelW_ = 0, pixelH_ = 0;
  std::vector<uint32_t> framebuffer_;
  bool framebufferDirty_ = true;
  std::vector<PixelRect> noChange_;

  double lastFrameTime_ = 0.0;
  int mouseButtonsDown_ = 0;
  float peakShown_ = -120.0f, rmsShown_ = -120.0f;
  uint64_t lastBlocks_ = 0;
};

} // namespace sdl2synth
