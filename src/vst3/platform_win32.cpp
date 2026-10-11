// Windows editor window: a child HWND inside the host's window. The framebuffer
// is copied into it with GDI (StretchDIBits); no OpenGL involved.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include "platform.h"

#include <cstring>

// Set by the SDK's dllmain.cpp to this DLL's HINSTANCE.
extern void* moduleHandle;

using namespace Steinberg;

namespace sdl2synth {

namespace {

constexpr wchar_t kWindowClass[] = L"SDL2SynthEditorWindow";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kFrameIntervalMs = 33; // ~30 fps

int g_classRefs = 0;

HINSTANCE moduleInstance() { return reinterpret_cast<HINSTANCE>(moduleHandle); }

class Win32Window;
LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool acquireWindowClass() {
  if (g_classRefs++ > 0)
    return true;
  WNDCLASSEXW wc;
  std::memset(&wc, 0, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.style = CS_DBLCLKS;
  wc.lpfnWndProc = windowProc;
  wc.hInstance = moduleInstance();
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = kWindowClass;
  if (!RegisterClassExW(&wc)) {
    --g_classRefs;
    return false;
  }
  return true;
}

void releaseWindowClass() {
  if (--g_classRefs == 0)
    UnregisterClassW(kWindowClass, moduleInstance());
}

class Win32Window : public PlatformWindow {
public:
  explicit Win32Window(PlatformEvents* events) : events_(events) {}

  ~Win32Window() override {
    if (hwnd_) {
      KillTimer(hwnd_, kTimerId);
      SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
      DestroyWindow(hwnd_);
    }
    if (classAcquired_)
      releaseWindowClass();
  }

  bool create(HWND parent, int width, int height) {
    classAcquired_ = acquireWindowClass();
    if (!classAcquired_)
      return false;
    width_ = width;
    height_ = height;
    hwnd_ = CreateWindowExW(0, kWindowClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0,
                            width, height, parent, nullptr, moduleInstance(), this);
    if (!hwnd_)
      return false;
    SetTimer(hwnd_, kTimerId, kFrameIntervalMs, nullptr);
    return true;
  }

  void setPixelSize(int width, int height) override {
    width_ = width;
    height_ = height;
    lastPixels_ = nullptr; // the old buffer no longer matches
    SetWindowPos(hwnd_, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }

  void present(const uint32_t* pixels, int width, int height, const PixelRect* rects, int rectCount) override {
    if (!hwnd_ || width != width_ || height != height_)
      return;
    lastPixels_ = pixels;
    HDC dc = GetDC(hwnd_);
    if (dc) {
      for (int i = 0; i < rectCount; ++i)
        blit(dc, rects[i].x, rects[i].y, rects[i].w, rects[i].h);
      ReleaseDC(hwnd_, dc);
    }
  }

  LRESULT handle(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
      case WM_ERASEBKGND:
        return 1;
      case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (lastPixels_) {
          const RECT& r = ps.rcPaint;
          blit(dc, r.left, r.top, r.right - r.left, r.bottom - r.top);
        }
        EndPaint(hwnd, &ps);
        return 0;
      }
      case WM_TIMER:
        if (wParam == kTimerId)
          events_->onFrameTimer();
        return 0;
      case WM_MOUSEMOVE:
        if (!tracking_) {
          TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
          TrackMouseEvent(&tme);
          tracking_ = true;
        }
        events_->onShift((wParam & MK_SHIFT) != 0);
        events_->onMouseMove((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam));
        return 0;
      case WM_MOUSELEAVE:
        tracking_ = false;
        events_->onMouseLeave();
        return 0;
      case WM_LBUTTONDOWN:
      case WM_LBUTTONDBLCLK:
        return button(hwnd, 0, true, wParam, lParam);
      case WM_LBUTTONUP:
        return button(hwnd, 0, false, wParam, lParam);
      case WM_RBUTTONDOWN:
      case WM_RBUTTONDBLCLK:
        return button(hwnd, 1, true, wParam, lParam);
      case WM_RBUTTONUP:
        return button(hwnd, 1, false, wParam, lParam);
      case WM_MBUTTONDOWN:
      case WM_MBUTTONDBLCLK:
        return button(hwnd, 2, true, wParam, lParam);
      case WM_MBUTTONUP:
        return button(hwnd, 2, false, wParam, lParam);
      case WM_MOUSEWHEEL:
        events_->onMouseWheel(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
        return 0;
      case WM_MOUSEHWHEEL:
        events_->onMouseWheel((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, 0.0f);
        return 0;
      case WM_CAPTURECHANGED:
        // Lost capture without a button-up (e.g. a host dialog popped up).
        for (int b = 0; b < 3; ++b) {
          if (buttonsDown_ & (1 << b))
            events_->onMouseButton(b, false);
        }
        buttonsDown_ = 0;
        return 0;
      default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

private:
  LRESULT button(HWND hwnd, int index, bool down, WPARAM wParam, LPARAM lParam) {
    events_->onShift((wParam & MK_SHIFT) != 0);
    events_->onMouseMove((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam));
    if (down) {
      if (buttonsDown_ == 0)
        SetCapture(hwnd);
      buttonsDown_ |= 1 << index;
    } else {
      buttonsDown_ &= ~(1 << index);
      if (buttonsDown_ == 0 && GetCapture() == hwnd)
        ReleaseCapture();
    }
    events_->onMouseButton(index, down);
    return 0;
  }

  void blit(HDC dc, int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > width_) w = width_ - x;
    if (y + h > height_) h = height_ - y;
    if (w <= 0 || h <= 0)
      return;
    BITMAPINFO bmi;
    std::memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -height_; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(dc, x, y, w, h, x, y, w, h, lastPixels_, &bmi, DIB_RGB_COLORS, SRCCOPY);
  }

  PlatformEvents* events_;
  HWND hwnd_ = nullptr;
  bool classAcquired_ = false;
  bool tracking_ = false;
  int buttonsDown_ = 0;
  int width_ = 0, height_ = 0;
  const uint32_t* lastPixels_ = nullptr;
};

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_NCCREATE) {
    const CREATESTRUCTW* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }
  Win32Window* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (self)
    return self->handle(hwnd, msg, wParam, lParam);
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

bool isPlatformTypeSupported(FIDString type) {
  return type && std::strcmp(type, kPlatformTypeHWND) == 0;
}

std::unique_ptr<PlatformWindow> createPlatformWindow(void* parent, FIDString type, IPlugFrame* /*frame*/,
                                                     PlatformEvents* events, int pixelWidth, int pixelHeight) {
  if (!isPlatformTypeSupported(type) || !parent)
    return nullptr;
  std::unique_ptr<Win32Window> window(new Win32Window(events));
  if (!window->create(reinterpret_cast<HWND>(parent), pixelWidth, pixelHeight))
    return nullptr;
  return window;
}

} // namespace sdl2synth
