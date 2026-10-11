#pragma once

// Thin per-platform layer for the editor: owns the native child window the
// host embeds, turns native input into simple callbacks, drives a ~30 Hz
// frame timer and blits the software-rendered framebuffer into the window.
// Implementations: platform_x11.cpp, platform_win32.cpp, platform_cocoa.mm.

#include "pixel_rect.h"

#include "pluginterfaces/base/ftypes.h"
#include "pluginterfaces/gui/iplugview.h"

#include <cstdint>
#include <memory>

namespace sdl2synth {

// Mouse buttons use ImGui numbering: 0 = left, 1 = right, 2 = middle.
class PlatformEvents {
public:
  virtual ~PlatformEvents() = default;
  virtual void onFrameTimer() = 0;
  virtual void onMouseMove(float x, float y) = 0;          // window pixels
  virtual void onMouseButton(int button, bool down) = 0;
  virtual void onMouseWheel(float dx, float dy) = 0;       // notches, +y = up
  virtual void onMouseLeave() = 0;
  virtual void onShift(bool down) = 0;
  virtual void onPaint() = 0;                              // window was exposed
};

class PlatformWindow {
public:
  virtual ~PlatformWindow() = default;

  // Resizes the native window (pixels).
  virtual void setPixelSize(int width, int height) = 0;

  // Copies the given regions of a width*height 0xAARRGGBB framebuffer into the
  // window. Everything outside `rects` is already up to date.
  virtual void present(const uint32_t* pixels, int width, int height, const PixelRect* rects, int rectCount) = 0;

  // Pixels per logical unit that the platform itself dictates (macOS backing
  // scale). Platforms where the host communicates DPI return 0 and the editor
  // uses the host-provided factor instead.
  virtual float platformScale() const { return 0.0f; }
};

// Creates the embedded child window inside `parent` (HWND / NSView* / X11
// window id, depending on `type`). Returns null if the platform cannot host
// the editor (e.g. no run loop on Linux).
std::unique_ptr<PlatformWindow> createPlatformWindow(void* parent, Steinberg::FIDString type,
                                                     Steinberg::IPlugFrame* frame, PlatformEvents* events,
                                                     int pixelWidth, int pixelHeight);

// True when the given platform type string names a window kind this build can embed.
bool isPlatformTypeSupported(Steinberg::FIDString type);

// Whether view sizes exchanged with the host are logical units (macOS) or pixels.
constexpr bool kHostSizesAreLogical =
#if defined(__APPLE__)
    true;
#else
    false;
#endif

} // namespace sdl2synth
