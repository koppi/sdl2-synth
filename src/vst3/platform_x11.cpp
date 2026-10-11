// Linux editor window: an X11 child window embedded into the host's window
// (VST3's "X11EmbedWindowID"). The host provides the event loop through
// IRunLoop; we register our X connection and a frame timer with it.

#include "platform.h"

#include "base/source/fobject.h"
#include "pluginterfaces/gui/iplugview.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <cstring>
#include <vector>

using namespace Steinberg;

namespace sdl2synth {

namespace {

constexpr Linux::TimerInterval kFrameIntervalMs = 33; // ~30 fps

class X11Window;

// Receives callbacks from the host's run loop.
class RunLoopHandler : public FObject, public Linux::IEventHandler, public Linux::ITimerHandler {
public:
  explicit RunLoopHandler(X11Window* owner) : owner_(owner) {}
  void detach() { owner_ = nullptr; }

  void PLUGIN_API onFDIsSet(Linux::FileDescriptor fd) override;
  void PLUGIN_API onTimer() override;

  OBJ_METHODS(RunLoopHandler, FObject)
  DEFINE_INTERFACES
    DEF_INTERFACE(Linux::IEventHandler)
    DEF_INTERFACE(Linux::ITimerHandler)
  END_DEFINE_INTERFACES(FObject)
  REFCOUNT_METHODS(FObject)

private:
  X11Window* owner_;
};

class X11Window : public PlatformWindow {
public:
  X11Window(PlatformEvents* events) : events_(events) {}

  ~X11Window() override {
    if (runLoop_ && handler_) {
      runLoop_->unregisterTimer(handler_);
      runLoop_->unregisterEventHandler(handler_);
    }
    if (handler_)
      handler_->detach();
    if (image_) {
      image_->data = nullptr; // the pixels belong to the caller
      XDestroyImage(image_);
    }
    if (display_) {
      if (gc_)
        XFreeGC(display_, gc_);
      if (window_)
        XDestroyWindow(display_, window_);
      XFlush(display_);
      XCloseDisplay(display_);
    }
  }

  bool create(void* parentHandle, IPlugFrame* frame, int width, int height) {
    if (!frame)
      return false;
    runLoop_ = frame;
    if (!runLoop_)
      return false; // the host must provide an IRunLoop on Linux

    display_ = XOpenDisplay(nullptr);
    if (!display_)
      return false;

    const Window parent = (Window)(uintptr_t)parentHandle;
    XWindowAttributes parentAttrs;
    if (!XGetWindowAttributes(display_, parent, &parentAttrs))
      return false;
    visual_ = parentAttrs.visual;
    depth_ = parentAttrs.depth;

    XSetWindowAttributes attrs;
    std::memset(&attrs, 0, sizeof(attrs));
    attrs.background_pixel = BlackPixel(display_, DefaultScreen(display_));
    attrs.border_pixel = 0;
    attrs.colormap = XCreateColormap(display_, parent, visual_, AllocNone);
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                       EnterWindowMask | LeaveWindowMask | KeyPressMask | KeyReleaseMask | FocusChangeMask;
    window_ = XCreateWindow(display_, parent, 0, 0, (unsigned)width, (unsigned)height, 0, depth_, InputOutput,
                            visual_, CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &attrs);
    if (!window_)
      return false;
    gc_ = XCreateGC(display_, window_, 0, nullptr);
    XMapWindow(display_, window_);
    XFlush(display_);

    width_ = width;
    height_ = height;
    swapRedBlue_ = visual_->red_mask == 0x0000FF && visual_->blue_mask == 0xFF0000;
    recreateImage();

    handler_ = owned(new RunLoopHandler(this));
    runLoop_->registerEventHandler(handler_, ConnectionNumber(display_));
    runLoop_->registerTimer(handler_, kFrameIntervalMs);
    return true;
  }

  void setPixelSize(int width, int height) override {
    if (width == width_ && height == height_)
      return;
    width_ = width;
    height_ = height;
    XResizeWindow(display_, window_, (unsigned)width, (unsigned)height);
    recreateImage();
    XFlush(display_);
  }

  void present(const uint32_t* pixels, int width, int height, const PixelRect* rects, int rectCount) override {
    if (!display_ || !window_ || !image_ || width != width_ || height != height_)
      return;
    const uint32_t* source = pixels;
    if (swapRedBlue_) {
      swapped_.resize((size_t)width * (size_t)height);
      for (int i = 0; i < rectCount; ++i) {
        for (int y = rects[i].y; y < rects[i].y + rects[i].h; ++y) {
          for (int x = rects[i].x; x < rects[i].x + rects[i].w; ++x) {
            const uint32_t p = pixels[(size_t)y * (size_t)width + (size_t)x];
            swapped_[(size_t)y * (size_t)width + (size_t)x] = (p & 0xFF00FF00u) | ((p & 0xFF) << 16) | ((p >> 16) & 0xFF);
          }
        }
      }
      source = swapped_.data();
    }
    image_->data = reinterpret_cast<char*>(const_cast<uint32_t*>(source));
    for (int i = 0; i < rectCount; ++i) {
      XPutImage(display_, window_, gc_, image_, rects[i].x, rects[i].y, rects[i].x, rects[i].y,
                (unsigned)rects[i].w, (unsigned)rects[i].h);
    }
    XFlush(display_);
    image_->data = nullptr;
  }

  // Drains pending X events (called when the connection becomes readable and
  // before every frame).
  void processEvents() {
    if (!display_)
      return;
    while (XPending(display_) > 0) {
      XEvent e;
      XNextEvent(display_, &e);
      switch (e.type) {
        case Expose:
          if (e.xexpose.count == 0)
            events_->onPaint();
          break;
        case MotionNotify:
          events_->onShift((e.xmotion.state & ShiftMask) != 0);
          events_->onMouseMove((float)e.xmotion.x, (float)e.xmotion.y);
          break;
        case EnterNotify:
          events_->onMouseMove((float)e.xcrossing.x, (float)e.xcrossing.y);
          break;
        case LeaveNotify:
          if (e.xcrossing.mode == NotifyNormal)
            events_->onMouseLeave();
          break;
        case ButtonPress:
        case ButtonRelease: {
          const bool down = e.type == ButtonPress;
          events_->onShift((e.xbutton.state & ShiftMask) != 0);
          events_->onMouseMove((float)e.xbutton.x, (float)e.xbutton.y);
          switch (e.xbutton.button) {
            case Button1: events_->onMouseButton(0, down); break;
            case Button3: events_->onMouseButton(1, down); break;
            case Button2: events_->onMouseButton(2, down); break;
            case Button4: if (down) events_->onMouseWheel(0.0f, 1.0f); break;
            case Button5: if (down) events_->onMouseWheel(0.0f, -1.0f); break;
            case 6: if (down) events_->onMouseWheel(-1.0f, 0.0f); break;
            case 7: if (down) events_->onMouseWheel(1.0f, 0.0f); break;
            default: break;
          }
          break;
        }
        case KeyPress:
        case KeyRelease: {
          const KeySym sym = XLookupKeysym(&e.xkey, 0);
          if (sym == XK_Shift_L || sym == XK_Shift_R)
            events_->onShift(e.type == KeyPress);
          break;
        }
        default:
          break;
      }
    }
  }

  void onTimer() {
    processEvents();
    events_->onFrameTimer();
  }

private:
  void recreateImage() {
    if (image_) {
      image_->data = nullptr;
      XDestroyImage(image_);
      image_ = nullptr;
    }
    image_ = XCreateImage(display_, visual_, (unsigned)depth_, ZPixmap, 0, nullptr, (unsigned)width_,
                          (unsigned)height_, 32, 0);
  }

  PlatformEvents* events_;
  FUnknownPtr<Linux::IRunLoop> runLoop_;
  IPtr<RunLoopHandler> handler_;
  Display* display_ = nullptr;
  Window window_ = 0;
  GC gc_ = nullptr;
  Visual* visual_ = nullptr;
  int depth_ = 24;
  XImage* image_ = nullptr;
  int width_ = 0, height_ = 0;
  bool swapRedBlue_ = false;
  std::vector<uint32_t> swapped_;
};

void PLUGIN_API RunLoopHandler::onFDIsSet(Linux::FileDescriptor) {
  if (owner_)
    owner_->processEvents();
}

void PLUGIN_API RunLoopHandler::onTimer() {
  if (owner_)
    owner_->onTimer();
}

} // namespace

bool isPlatformTypeSupported(FIDString type) {
  return type && std::strcmp(type, kPlatformTypeX11EmbedWindowID) == 0;
}

std::unique_ptr<PlatformWindow> createPlatformWindow(void* parent, FIDString type, IPlugFrame* frame,
                                                     PlatformEvents* events, int pixelWidth, int pixelHeight) {
  if (!isPlatformTypeSupported(type))
    return nullptr;
  std::unique_ptr<X11Window> window(new X11Window(events));
  if (!window->create(parent, frame, pixelWidth, pixelHeight))
    return nullptr;
  return window;
}

} // namespace sdl2synth
