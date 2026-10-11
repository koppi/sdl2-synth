// macOS editor window: an NSView added to the host's parent view. The
// framebuffer is drawn into it with CoreGraphics; no OpenGL/Metal involved.

#import <Cocoa/Cocoa.h>

#include "platform.h"

#include <cstring>

using namespace Steinberg;

@interface SynthEditorNSView : NSView {
  sdl2synth::PlatformEvents* _events; // not owned; cleared by -stop
  const uint32_t* _pixels;
  int _pixelWidth;
  int _pixelHeight;
  NSTimer* _timer;
  NSTrackingArea* _tracking;
}
- (instancetype)initWithFrame:(NSRect)frame events:(sdl2synth::PlatformEvents*)events;
- (void)setFramebuffer:(const uint32_t*)pixels width:(int)width height:(int)height;
- (void)invalidatePixelRect:(NSRect)pixelRect;
- (CGFloat)backingScale;
- (void)stop;
@end

@implementation SynthEditorNSView

- (instancetype)initWithFrame:(NSRect)frame events:(sdl2synth::PlatformEvents*)events {
  self = [super initWithFrame:frame];
  if (self) {
    _events = events;
    _pixels = nullptr;
    _pixelWidth = 0;
    _pixelHeight = 0;
    _timer = [NSTimer timerWithTimeInterval:1.0 / 30.0 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:_timer forMode:NSRunLoopCommonModes];
  }
  return self;
}

- (void)stop {
  [_timer invalidate];
  _timer = nil;
  _events = nullptr;
  _pixels = nullptr;
}

- (CGFloat)backingScale {
  NSWindow* window = [self window];
  return window ? [window backingScaleFactor] : 1.0;
}

- (void)setFramebuffer:(const uint32_t*)pixels width:(int)width height:(int)height {
  _pixels = pixels;
  _pixelWidth = width;
  _pixelHeight = height;
}

// Marks a framebuffer region (pixels) as needing redraw.
- (void)invalidatePixelRect:(NSRect)pixelRect {
  const CGFloat scale = [self backingScale];
  [self setNeedsDisplayInRect:NSMakeRect(pixelRect.origin.x / scale, pixelRect.origin.y / scale,
                                         pixelRect.size.width / scale, pixelRect.size.height / scale)];
}

- (BOOL)isFlipped { return YES; }
- (BOOL)isOpaque { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { return YES; }

- (void)tick:(NSTimer*)timer {
  if (_events)
    _events->onFrameTimer();
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  if (_tracking)
    [self removeTrackingArea:_tracking];
  _tracking = [[NSTrackingArea alloc]
      initWithRect:[self bounds]
           options:(NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways |
                    NSTrackingInVisibleRect)
             owner:self
          userInfo:nil];
  [self addTrackingArea:_tracking];
}

- (void)drawRect:(NSRect)dirtyRect {
  CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
  if (!_pixels || _pixelWidth <= 0 || _pixelHeight <= 0) {
    CGContextSetRGBFillColor(context, 0, 0, 0, 1);
    CGContextFillRect(context, NSRectToCGRect([self bounds]));
    return;
  }
  CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  CGDataProviderRef provider =
      CGDataProviderCreateWithData(nullptr, _pixels, (size_t)_pixelWidth * (size_t)_pixelHeight * 4, nullptr);
  CGImageRef image = CGImageCreate((size_t)_pixelWidth, (size_t)_pixelHeight, 8, 32, (size_t)_pixelWidth * 4,
                                   colorSpace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst, provider,
                                   nullptr, false, kCGRenderingIntentDefault);
  if (image) {
    // The view is flipped (origin top-left); CoreGraphics images are drawn
    // bottom-up, so flip once more to get an upright picture.
    const CGRect bounds = NSRectToCGRect([self bounds]);
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, 0, bounds.size.height);
    CGContextScaleCTM(context, 1.0, -1.0);
    CGContextDrawImage(context, CGRectMake(0, 0, bounds.size.width, bounds.size.height), image);
    CGContextRestoreGState(context);
    CGImageRelease(image);
  }
  CGDataProviderRelease(provider);
  CGColorSpaceRelease(colorSpace);
}

// --- Input. Coordinates are converted from points to framebuffer pixels. ---

- (NSPoint)pixelPoint:(NSEvent*)event {
  NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
  const CGFloat scale = [self backingScale];
  return NSMakePoint(p.x * scale, p.y * scale);
}

- (void)reportMouse:(NSEvent*)event {
  if (!_events)
    return;
  _events->onShift(([event modifierFlags] & NSEventModifierFlagShift) != 0);
  const NSPoint p = [self pixelPoint:event];
  _events->onMouseMove((float)p.x, (float)p.y);
}

- (void)mouseMoved:(NSEvent*)event { [self reportMouse:event]; }
- (void)mouseDragged:(NSEvent*)event { [self reportMouse:event]; }
- (void)rightMouseDragged:(NSEvent*)event { [self reportMouse:event]; }
- (void)otherMouseDragged:(NSEvent*)event { [self reportMouse:event]; }

- (void)mouseExited:(NSEvent*)event {
  if (_events)
    _events->onMouseLeave();
}

- (void)mouseDown:(NSEvent*)event {
  [self reportMouse:event];
  if (_events)
    _events->onMouseButton(0, true);
}
- (void)mouseUp:(NSEvent*)event {
  [self reportMouse:event];
  if (_events)
    _events->onMouseButton(0, false);
}
- (void)rightMouseDown:(NSEvent*)event {
  [self reportMouse:event];
  if (_events)
    _events->onMouseButton(1, true);
}
- (void)rightMouseUp:(NSEvent*)event {
  [self reportMouse:event];
  if (_events)
    _events->onMouseButton(1, false);
}
- (void)otherMouseDown:(NSEvent*)event {
  [self reportMouse:event];
  if (_events)
    _events->onMouseButton(2, true);
}
- (void)otherMouseUp:(NSEvent*)event {
  [self reportMouse:event];
  if (_events)
    _events->onMouseButton(2, false);
}

- (void)scrollWheel:(NSEvent*)event {
  if (!_events)
    return;
  [self reportMouse:event];
  CGFloat dx = [event scrollingDeltaX];
  CGFloat dy = [event scrollingDeltaY];
  if ([event hasPreciseScrollingDeltas]) {
    // Trackpads report pixels; scale them to roughly one notch per 8 points.
    dx /= 8.0;
    dy /= 8.0;
  }
  _events->onMouseWheel((float)dx, (float)dy);
}

- (void)flagsChanged:(NSEvent*)event {
  if (_events)
    _events->onShift(([event modifierFlags] & NSEventModifierFlagShift) != 0);
  [super flagsChanged:event];
}

@end

namespace sdl2synth {

namespace {

class CocoaWindow : public PlatformWindow {
public:
  explicit CocoaWindow(PlatformEvents* events) : events_(events) {}

  ~CocoaWindow() override {
    if (view_) {
      [view_ stop];
      [view_ removeFromSuperview];
      view_ = nil;
    }
  }

  bool create(void* parent, int width, int height) {
    NSView* parentView = (__bridge NSView*)parent;
    if (!parentView)
      return false;
    // Created at 1x; the editor re-sizes us once the backing scale is known.
    view_ = [[SynthEditorNSView alloc] initWithFrame:NSMakeRect(0, 0, width, height) events:events_];
    if (!view_)
      return false;
    [parentView addSubview:view_];
    return true;
  }

  void setPixelSize(int width, int height) override {
    const CGFloat scale = [view_ backingScale];
    [view_ setFrameSize:NSMakeSize(width / scale, height / scale)];
    [view_ setFramebuffer:nullptr width:0 height:0];
    [view_ setNeedsDisplay:YES];
  }

  void present(const uint32_t* pixels, int width, int height, const PixelRect* rects, int rectCount) override {
    [view_ setFramebuffer:pixels width:width height:height];
    for (int i = 0; i < rectCount; ++i)
      [view_ invalidatePixelRect:NSMakeRect(rects[i].x, rects[i].y, rects[i].w, rects[i].h)];
  }

  float platformScale() const override { return (float)[view_ backingScale]; }

private:
  PlatformEvents* events_;
  SynthEditorNSView* view_ = nil;
};

} // namespace

bool isPlatformTypeSupported(FIDString type) {
  return type && std::strcmp(type, kPlatformTypeNSView) == 0;
}

std::unique_ptr<PlatformWindow> createPlatformWindow(void* parent, FIDString type, IPlugFrame* /*frame*/,
                                                     PlatformEvents* events, int pixelWidth, int pixelHeight) {
  if (!isPlatformTypeSupported(type))
    return nullptr;
  std::unique_ptr<CocoaWindow> window(new CocoaWindow(events));
  if (!window->create(parent, pixelWidth, pixelHeight))
    return nullptr;
  return window;
}

} // namespace sdl2synth
