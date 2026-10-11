#include "editor_core.h"

#include "controller.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>

namespace sdl2synth {

namespace {

double nowSeconds() {
  using Clock = std::chrono::steady_clock;
  static const Clock::time_point start = Clock::now();
  return std::chrono::duration<double>(Clock::now() - start).count();
}

constexpr uint32_t kBackground = 0xFF000000u;
constexpr double kGestureTimeout = 0.30; // seconds without change before an edit gesture ends

} // namespace

EditorCore::EditorCore(SynthController* controller) : controller_(controller) {
  viz_ = controller_->viz();

  // The model is a Synth that is never rendered: the shared UI edits it in
  // place and we mirror its values to and from the host.
  model_.reset(new Synth);
  synth_core_init(model_.get(), 8000, 1);
  const std::vector<ParamDef>& defs = params();
  state_.assign(defs.size(), ParamState());
  for (size_t i = 0; i < defs.size(); ++i) {
    writePlain(*model_, defs[i], defs[i].def);
    state_[i].plain = readPlain(*model_, defs[i]);
  }

  context_ = ImGui::CreateContext();
  ImGui::SetCurrentContext(context_);
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
  ImGui::StyleColorsDark();

  options_.scope = viz_ ? &viz_->scope : nullptr;
  options_.show_host_sync = true;

  lastFrameTime_ = nowSeconds();
  resizeFramebuffer();
}

EditorCore::~EditorCore() {
  finishEdits();
  if (context_) {
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    // Mirror what ImGui's stock renderer backends do on shutdown.
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
      if (tex->RefCount == 1) {
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
      }
    }
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset);
    ImGui::DestroyContext(context_);
  }
  if (model_)
    synth_core_shutdown(model_.get());
}

void EditorCore::setLogicalSize(int width, int height) {
  logicalW_ = std::max(width, 1);
  logicalH_ = std::max(height, 1);
  resizeFramebuffer();
}

void EditorCore::setScale(float scale) {
  scale_ = std::min(std::max(scale, 0.5f), 4.0f);
  resizeFramebuffer();
}

void EditorCore::resizeFramebuffer() {
  pixelW_ = std::max(1, (int)std::lround(logicalW_ * scale_));
  pixelH_ = std::max(1, (int)std::lround(logicalH_ * scale_));
  framebuffer_.assign((size_t)pixelW_ * (size_t)pixelH_, kBackground);
  framebufferDirty_ = true;
}

void EditorCore::mouseMove(float x, float y) {
  ImGui::SetCurrentContext(context_);
  ImGui::GetIO().AddMousePosEvent(x / scale_, y / scale_);
}

void EditorCore::mouseButton(int button, bool down) {
  ImGui::SetCurrentContext(context_);
  ImGui::GetIO().AddMouseButtonEvent(button, down);
  if (button >= 0 && button < 8) {
    if (down)
      mouseButtonsDown_ |= 1 << button;
    else
      mouseButtonsDown_ &= ~(1 << button);
  }
}

void EditorCore::mouseWheel(float dx, float dy) {
  ImGui::SetCurrentContext(context_);
  ImGui::GetIO().AddMouseWheelEvent(dx, dy);
}

void EditorCore::mouseLeave() {
  ImGui::SetCurrentContext(context_);
  ImGui::GetIO().AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

void EditorCore::shift(bool down) {
  ImGui::SetCurrentContext(context_);
  ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, down);
}

const std::vector<PixelRect>& EditorCore::frame(bool forceAll) {
  ImGui::SetCurrentContext(context_);
  const double now = nowSeconds();
  const float dt = (float)std::min(std::max(now - lastFrameTime_, 0.001), 0.1);
  lastFrameTime_ = now;

  syncFromHost();

  // Level meters: only trust the processor's numbers while audio is running.
  if (viz_) {
    const uint64_t blocks = viz_->blocks.load(std::memory_order_relaxed);
    const bool running = blocks != lastBlocks_;
    lastBlocks_ = blocks;
    const float peak = running ? viz_->peakDb.load() : -120.0f;
    const float rms = running ? viz_->rmsDb.load() : -120.0f;
    peakShown_ = std::max(peak, peakShown_ - 40.0f * dt);
    rmsShown_ = std::max(rms, rmsShown_ - 20.0f * dt);
    model_->mixer.peak_level = peakShown_;
    model_->mixer.rms_level = rmsShown_;
  }

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2((float)logicalW_, (float)logicalH_);
  io.DisplayFramebufferScale = ImVec2(scale_, scale_);
  io.DeltaTime = dt;

  ImGui::NewFrame();
  ui_.draw(model_.get(), (float)logicalW_, (float)logicalH_, options_);
  ImGui::Render();

  ImDrawData* drawData = ImGui::GetDrawData();
  renderer_.updateTextures(drawData);
  syncToHost(now);

  const bool all = forceAll || framebufferDirty_;
  framebufferDirty_ = false;
  return renderer_.render(drawData, framebuffer_.data(), pixelW_, pixelH_, kBackground, all);
}

void EditorCore::syncFromHost() {
  const std::vector<ParamDef>& defs = params();
  for (size_t i = 0; i < defs.size(); ++i) {
    ParamState& s = state_[i];
    if (s.inGesture)
      continue; // the user is editing it; do not fight them
    const double norm = controller_->getParamNormalized(defs[i].id);
    if (norm == s.normalized)
      continue;
    const float plain = (float)normalizedToPlain(defs[i], norm);
    writePlain(*model_, defs[i], plain);
    s.plain = readPlain(*model_, defs[i]);
    s.normalized = norm;
  }
}

void EditorCore::syncToHost(double now) {
  const std::vector<ParamDef>& defs = params();
  for (size_t i = 0; i < defs.size(); ++i) {
    ParamState& s = state_[i];
    float v = readPlain(*model_, defs[i]);
    if (!std::isfinite(v) || v != clampPlain(defs[i], v)) {
      v = std::isfinite(v) ? clampPlain(defs[i], v) : s.plain;
      writePlain(*model_, defs[i], v);
    }
    if (v != s.plain)
      reportEdit(i, v, now);
  }

  // Close gestures once the value has been still for a moment.
  if (mouseButtonsDown_ == 0) {
    for (size_t i = 0; i < defs.size(); ++i) {
      ParamState& s = state_[i];
      if (s.inGesture && now - s.lastEditTime > kGestureTimeout) {
        controller_->endEdit(defs[i].id);
        s.inGesture = false;
      }
    }
  }
}

void EditorCore::reportEdit(size_t index, float plain, double now) {
  const ParamDef& def = params()[index];
  ParamState& s = state_[index];
  const double norm = plainToNormalized(def, plain);
  if (!s.inGesture) {
    controller_->beginEdit(def.id);
    s.inGesture = true;
  }
  controller_->setParamNormalized(def.id, norm);
  controller_->performEdit(def.id, norm);
  s.plain = plain;
  s.normalized = norm;
  s.lastEditTime = now;
}

void EditorCore::finishEdits() {
  const std::vector<ParamDef>& defs = params();
  for (size_t i = 0; i < defs.size() && i < state_.size(); ++i) {
    if (state_[i].inGesture) {
      controller_->endEdit(defs[i].id);
      state_[i].inGesture = false;
    }
  }
}

} // namespace sdl2synth
