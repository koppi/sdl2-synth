#include "editor.h"

#include "controller.h"
#include "editor_core.h"
#include "platform.h"

#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "public.sdk/source/common/pluginview.h"

#include <algorithm>
#include <cmath>

using namespace Steinberg;

namespace sdl2synth {

namespace {

constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 860;
constexpr int kMinWidth = 800;
constexpr int kMinHeight = 500;

// The size the user last chose, so a re-opened editor keeps it.
int g_lastWidth = kDefaultWidth;
int g_lastHeight = kDefaultHeight;

} // namespace

// One open editor: the platform window plus the UI running inside it.
class EditorSession : public PlatformEvents {
public:
  EditorSession(SynthController* controller, void* parent, FIDString type, IPlugFrame* frame, int logicalWidth,
                int logicalHeight, float hostScale)
  : core_(controller), hostScale_(hostScale) {
    core_.setLogicalSize(logicalWidth, logicalHeight);
    scale_ = kHostSizesAreLogical ? 1.0f : hostScale;
    core_.setScale(scale_);
    window_ = createPlatformWindow(parent, type, frame, this, core_.pixelWidth(), core_.pixelHeight());
    if (window_) {
      refreshScale();
      renderAndPresent(true);
    }
  }

  bool ok() const { return window_ != nullptr; }

  void resizeLogical(int width, int height) {
    core_.setLogicalSize(width, height);
    if (window_) {
      window_->setPixelSize(core_.pixelWidth(), core_.pixelHeight());
      renderAndPresent(true);
    }
  }

  void setHostScale(float scale) {
    hostScale_ = scale;
    if (!kHostSizesAreLogical)
      applyScale(scale);
  }

  // --- PlatformEvents ----------------------------------------------------------
  void onFrameTimer() override {
    refreshScale();
    renderAndPresent(false);
  }
  void onMouseMove(float x, float y) override { core_.mouseMove(x, y); }
  void onMouseButton(int button, bool down) override { core_.mouseButton(button, down); }
  void onMouseWheel(float dx, float dy) override { core_.mouseWheel(dx, dy); }
  void onMouseLeave() override { core_.mouseLeave(); }
  void onShift(bool down) override { core_.shift(down); }
  void onPaint() override { presentAll(); }

private:
  void presentAll() {
    if (!window_)
      return;
    const PixelRect all = {0, 0, core_.pixelWidth(), core_.pixelHeight()};
    window_->present(core_.pixels(), core_.pixelWidth(), core_.pixelHeight(), &all, 1);
  }

  void renderAndPresent(bool force) {
    const std::vector<PixelRect>& changed = core_.frame(force);
    if (!window_ || changed.empty())
      return;
    window_->present(core_.pixels(), core_.pixelWidth(), core_.pixelHeight(), changed.data(), (int)changed.size());
  }

  void applyScale(float scale) {
    if (scale == scale_)
      return;
    scale_ = scale;
    core_.setScale(scale_);
    if (window_)
      window_->setPixelSize(core_.pixelWidth(), core_.pixelHeight());
  }

  // On macOS the window knows its backing scale (Retina); elsewhere the host
  // tells us through setContentScaleFactor().
  void refreshScale() {
    if (!window_)
      return;
    const float platform = window_->platformScale();
    if (platform > 0.0f)
      applyScale(platform);
  }

  EditorCore core_;
  std::unique_ptr<PlatformWindow> window_;
  float scale_ = 1.0f;
  float hostScale_ = 1.0f;
};

class SynthEditorView : public CPluginView, public IPlugViewContentScaleSupport {
public:
  explicit SynthEditorView(SynthController* controller) : controller_(controller) {
    logicalW_ = g_lastWidth;
    logicalH_ = g_lastHeight;
  }
  ~SynthEditorView() override { session_.reset(); }

  tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {
    return sdl2synth::isPlatformTypeSupported(type) ? kResultTrue : kResultFalse;
  }

  tresult PLUGIN_API attached(void* parent, FIDString type) override {
    if (!parent || !sdl2synth::isPlatformTypeSupported(type))
      return kResultFalse;
    session_.reset();
    std::unique_ptr<EditorSession> session(
        new EditorSession(controller_, parent, type, plugFrame, logicalW_, logicalH_, hostScale_));
    if (!session->ok())
      return kResultFalse;
    session_ = std::move(session);
    return CPluginView::attached(parent, type);
  }

  tresult PLUGIN_API removed() override {
    session_.reset();
    return CPluginView::removed();
  }

  tresult PLUGIN_API getSize(ViewRect* size) override {
    if (!size)
      return kInvalidArgument;
    const float f = kHostSizesAreLogical ? 1.0f : hostScale_;
    *size = ViewRect(0, 0, (int32)std::lround(logicalW_ * f), (int32)std::lround(logicalH_ * f));
    return kResultTrue;
  }

  tresult PLUGIN_API onSize(ViewRect* size) override {
    if (!size)
      return kInvalidArgument;
    const float f = kHostSizesAreLogical ? 1.0f : hostScale_;
    logicalW_ = std::max(kMinWidth, (int)std::lround(size->getWidth() / f));
    logicalH_ = std::max(kMinHeight, (int)std::lround(size->getHeight() / f));
    g_lastWidth = logicalW_;
    g_lastHeight = logicalH_;
    if (session_)
      session_->resizeLogical(logicalW_, logicalH_);
    return kResultTrue;
  }

  tresult PLUGIN_API canResize() override { return kResultTrue; }

  tresult PLUGIN_API checkSizeConstraint(ViewRect* rect) override {
    if (!rect)
      return kInvalidArgument;
    const float f = kHostSizesAreLogical ? 1.0f : hostScale_;
    const int minW = (int)std::lround(kMinWidth * f);
    const int minH = (int)std::lround(kMinHeight * f);
    if (rect->getWidth() < minW)
      rect->right = rect->left + minW;
    if (rect->getHeight() < minH)
      rect->bottom = rect->top + minH;
    return kResultTrue;
  }

  // Windows / Linux: the host reports its DPI scale; macOS reports it through
  // the view's backing store instead.
  tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override {
    if (kHostSizesAreLogical || !(factor > 0.0f))
      return kResultFalse;
    hostScale_ = std::min(std::max(factor, 0.5f), 4.0f);
    if (session_)
      session_->setHostScale(hostScale_);
    if (plugFrame) {
      ViewRect rect;
      getSize(&rect);
      plugFrame->resizeView(this, &rect);
    }
    return kResultTrue;
  }

  DEFINE_INTERFACES
    DEF_INTERFACE(IPlugViewContentScaleSupport)
  END_DEFINE_INTERFACES(CPluginView)
  REFCOUNT_METHODS(CPluginView)

private:
  IPtr<SynthController> controller_;
  std::unique_ptr<EditorSession> session_;
  int logicalW_ = kDefaultWidth;
  int logicalH_ = kDefaultHeight;
  float hostScale_ = 1.0f;
};

IPlugView* createSynthEditorView(SynthController* controller) { return new SynthEditorView(controller); }

} // namespace sdl2synth
