// Minimal VST3 host used to smoke-test the built plug-in on every platform.
//
//   vst3_test_host <path/to/sdl2-synth.vst3> [--editor <out.ppm>] [--drag x,y,dy]
//
// Always: loads the module, instantiates component + controller through the
// SDK's PlugProvider, plays notes and checks the rendered audio, automation,
// MIDI CC mapping and state save/restore.
// Linux with --editor: also opens the editor in an X11 window (providing the
// IRunLoop that Linux hosts must offer), saves a screenshot, and with --drag
// performs a mouse drag and reports the parameter edits the editor sent.

#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"

#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#if defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <poll.h>
#include <time.h>
#endif

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
  std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what);
  if (!ok)
    ++g_failures;
}

// ---------------------------------------------------------------------------
// Host side objects
// ---------------------------------------------------------------------------
class ComponentHandler : public IComponentHandler {
public:
  struct Edit {
    ParamID id;
    ParamValue value;
  };
  std::vector<Edit> edits;
  int begins = 0, ends = 0, restarts = 0;

  tresult PLUGIN_API beginEdit(ParamID) override { ++begins; return kResultOk; }
  tresult PLUGIN_API performEdit(ParamID id, ParamValue value) override {
    edits.push_back({id, value});
    return kResultOk;
  }
  tresult PLUGIN_API endEdit(ParamID) override { ++ends; return kResultOk; }
  tresult PLUGIN_API restartComponent(int32) override { ++restarts; return kResultOk; }

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
    if (FUnknownPrivate::iidEqual(iid, IComponentHandler::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
      *obj = this;
      return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }
};

struct Plugin {
  VST3::Hosting::Module::Ptr module;
  std::unique_ptr<PlugProvider> provider;
  IPtr<IComponent> component;
  IPtr<IEditController> controller;
  FUnknownPtr<IAudioProcessor> processor;
  ComponentHandler handler;
  HostProcessData data;
  ParameterChanges paramChanges;
  EventList events;
  ProcessContext context{};
  std::vector<float> outL, outR;
  int blockSize = 512;
  double sampleRate = 48000.0;
};

bool loadPlugin(Plugin& p, const std::string& path, HostApplication& host) {
  std::string error;
  p.module = VST3::Hosting::Module::create(path, error);
  if (!p.module) {
    std::printf("cannot load module: %s\n", error.c_str());
    return false;
  }
  PluginContextFactory::instance().setPluginContext(&host);
  for (auto& info : p.module->getFactory().classInfos()) {
    if (info.category() == kVstAudioEffectClass) {
      p.provider = std::make_unique<PlugProvider>(p.module->getFactory(), info, true);
      break;
    }
  }
  if (!p.provider || !p.provider->initialize())
    return false;
  p.component = p.provider->getComponentPtr();
  p.controller = p.provider->getControllerPtr();
  p.processor = p.component;
  if (!p.component || !p.controller || !p.processor)
    return false;
  p.controller->setComponentHandler(&p.handler);
  return true;
}

bool setupAudio(Plugin& p, double sampleRate, int blockSize) {
  p.sampleRate = sampleRate;
  p.blockSize = blockSize;
  p.component->setActive(false);
  p.component->activateBus(kAudio, kOutput, 0, true);
  p.component->activateBus(kEvent, kInput, 0, true);
  SpeakerArrangement stereo = SpeakerArr::kStereo;
  if (p.processor->setBusArrangements(nullptr, 0, &stereo, 1) != kResultTrue)
    return false;
  ProcessSetup setup{kRealtime, kSample32, blockSize, sampleRate};
  if (p.processor->setupProcessing(setup) != kResultOk)
    return false;
  if (p.component->setActive(true) != kResultOk)
    return false;
  p.processor->setProcessing(true);
  p.data.unprepare();
  if (!p.data.prepare(*p.component, blockSize, kSample32))
    return false;
  p.data.numSamples = blockSize;
  p.context = ProcessContext();
  p.context.state = ProcessContext::kTempoValid | ProcessContext::kPlaying;
  p.context.tempo = 120.0;
  p.context.sampleRate = sampleRate;
  p.data.processContext = &p.context;
  p.data.inputEvents = &p.events;
  p.data.inputParameterChanges = &p.paramChanges;
  return true;
}

// Runs one block; the caller prepares p.events / p.paramChanges first.
void processBlock(Plugin& p) {
  p.data.numSamples = p.blockSize;
  p.processor->process(p.data);
  const float* l = p.data.outputs[0].channelBuffers32[0];
  const float* r = p.data.outputs[0].channelBuffers32[1];
  p.outL.insert(p.outL.end(), l, l + p.blockSize);
  p.outR.insert(p.outR.end(), r, r + p.blockSize);
  p.events.clear();
  p.paramChanges.clearQueue();
}

void addNote(Plugin& p, bool on, int pitch, float velocity, int offset) {
  Event e{};
  e.type = on ? Event::kNoteOnEvent : Event::kNoteOffEvent;
  e.sampleOffset = offset;
  if (on) {
    e.noteOn.channel = 0;
    e.noteOn.pitch = (int16)pitch;
    e.noteOn.velocity = velocity;
    e.noteOn.noteId = -1;
  } else {
    e.noteOff.channel = 0;
    e.noteOff.pitch = (int16)pitch;
    e.noteOff.velocity = 0.0f;
    e.noteOff.noteId = -1;
  }
  p.events.addEvent(e);
}

ParamID idOf(Plugin& p, const char* title) {
  ParameterInfo info{};
  for (int32 i = 0; i < p.controller->getParameterCount(); ++i) {
    p.controller->getParameterInfo(i, info);
    char name[128];
    for (int k = 0; k < 128; ++k) {
      name[k] = (char)info.title[k];
      if (!info.title[k])
        break;
    }
    if (std::strcmp(name, title) == 0)
      return info.id;
  }
  return kNoParamId;
}

void setParam(Plugin& p, ParamID id, double normalized, int offset = 0) {
  int32 index = 0;
  IParamValueQueue* q = p.paramChanges.addParameterData(id, index);
  int32 point = 0;
  if (q)
    q->addPoint(offset, normalized, point);
  p.controller->setParamNormalized(id, normalized);
}

float peak(const std::vector<float>& v, size_t from, size_t to) {
  float m = 0.0f;
  for (size_t i = from; i < to && i < v.size(); ++i)
    m = std::max(m, std::fabs(v[i]));
  return m;
}

bool allFinite(const std::vector<float>& v) {
  for (float f : v)
    if (!std::isfinite(f))
      return false;
  return true;
}

// ---------------------------------------------------------------------------
// Audio tests
// ---------------------------------------------------------------------------
void runAudioTests(Plugin& p) {
  std::printf("-- audio --\n");
  check(setupAudio(p, 48000.0, 512), "setup 48 kHz / 512");

  // Silence before any note.
  for (int i = 0; i < 4; ++i)
    processBlock(p);
  check(peak(p.outL, 0, p.outL.size()) < 1e-6f, "silent without notes");

  // Note-on in the middle of a block starts exactly there.
  p.outL.clear();
  p.outR.clear();
  addNote(p, true, 60, 0.9f, 100);
  processBlock(p);
  for (int i = 0; i < 30; ++i)
    processBlock(p);
  check(peak(p.outL, 0, 100) < 1e-6f, "no sound before the note's sample offset");
  check(peak(p.outL, 100, p.outL.size()) > 0.02f, "note produces audible output");
  check(allFinite(p.outL) && allFinite(p.outR), "output is finite");
  check(peak(p.outL, 0, p.outL.size()) <= 1.0001f, "output does not exceed 0 dBFS");

  // Release: tail dies out.
  addNote(p, false, 60, 0.0f, 0);
  processBlock(p);
  p.outL.clear();
  for (int i = 0; i < 200; ++i) // ~2 s
    processBlock(p);
  const size_t n = p.outL.size();
  check(peak(p.outL, n - 4096, n) < 0.01f, "sound decays after note-off");

  // Parameter automation: master volume to zero silences a held note.
  const ParamID master = idOf(p, "Master Volume");
  check(master != kNoParamId, "found 'Master Volume' parameter");
  p.outL.clear();
  addNote(p, true, 64, 1.0f, 0);
  for (int i = 0; i < 20; ++i)
    processBlock(p);
  const float loud = peak(p.outL, p.outL.size() - 2048, p.outL.size());
  setParam(p, master, 0.0);
  processBlock(p);
  p.outL.clear();
  for (int i = 0; i < 200; ++i) // let the reverb tail ring out (~2 s)
    processBlock(p);
  check(loud > 0.02f && peak(p.outL, p.outL.size() - 4096, p.outL.size()) < 1e-4f,
        "automating master volume to 0 silences output");
  setParam(p, master, 0.25);
  addNote(p, false, 64, 0.0f, 0);
  processBlock(p);

  // Pitch bend arrives through IMidiMapping.
  FUnknownPtr<IMidiMapping> mapping(p.controller);
  ParamID bendId = kNoParamId;
  check(mapping && mapping->getMidiControllerAssignment(0, 0, kPitchBend, bendId) == kResultTrue &&
            bendId != kNoParamId,
        "pitch bend is mapped for the host");
  ParamID ccId = kNoParamId;
  check(mapping && mapping->getMidiControllerAssignment(0, 0, 74, ccId) == kResultTrue,
        "CC74 is mapped for the host");

  // Other sample rates and block sizes keep working.
  const double rates[] = {44100.0, 96000.0};
  const int blocks[] = {64, 1024};
  for (int i = 0; i < 2; ++i) {
    char what[96];
    std::snprintf(what, sizeof(what), "renders at %.0f Hz with %d-sample blocks", rates[i], blocks[i]);
    p.outL.clear();
    p.outR.clear();
    bool ok = setupAudio(p, rates[i], blocks[i]);
    if (ok) {
      addNote(p, true, 67, 0.8f, 0);
      for (int b = 0; b < 12288 / blocks[i]; ++b)
        processBlock(p);
      ok = peak(p.outL, 0, p.outL.size()) > 0.02f && allFinite(p.outL);
    }
    check(ok, what);
  }
  setupAudio(p, 48000.0, 512);
}

// ---------------------------------------------------------------------------
// State tests
// ---------------------------------------------------------------------------
void runStateTests(Plugin& p) {
  std::printf("-- state --\n");
  const ParamID cutoff = idOf(p, "Filter Cutoff");
  const ParamID wave = idOf(p, "OSC1 Waveform");
  check(cutoff != kNoParamId && wave != kNoParamId, "found test parameters");

  setParam(p, cutoff, 0.8);
  setParam(p, wave, 1.0); // last waveform: NOISE
  processBlock(p);

  MemoryStream stream;
  check(p.component->getState(&stream) == kResultOk && stream.getSize() > 0, "getState writes data");

  setParam(p, cutoff, 0.1);
  setParam(p, wave, 0.0);
  processBlock(p);

  int64 pos = 0;
  stream.seek(0, IBStream::kIBSeekSet, &pos);
  check(p.component->setState(&stream) == kResultOk, "setState accepts the saved data");
  stream.seek(0, IBStream::kIBSeekSet, &pos);
  check(p.controller->setComponentState(&stream) == kResultOk, "controller reads the component state");
  check(std::fabs(p.controller->getParamNormalized(cutoff) - 0.8) < 1e-3 &&
            std::fabs(p.controller->getParamNormalized(wave) - 1.0) < 1e-3,
        "controller parameters restored from state");

  // Garbage must be rejected, not crash.
  char junk[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  MemoryStream bad(junk, sizeof(junk));
  check(p.component->setState(&bad) != kResultOk, "garbage state is rejected");

  // Every parameter must round-trip through its string form.
  bool strings = true;
  for (int32 i = 0; i < p.controller->getParameterCount(); ++i) {
    ParameterInfo info{};
    p.controller->getParameterInfo(i, info);
    String128 text;
    p.controller->getParamStringByValue(info.id, info.defaultNormalizedValue, text);
    ParamValue back = -1.0;
    if (text[0] == 0 || p.controller->getParamValueByString(info.id, text, back) != kResultOk) {
      strings = false;
      break;
    }
  }
  check(strings, "all parameters convert to and from strings");
}

#if defined(__linux__)
// ---------------------------------------------------------------------------
// Editor test (X11)
// ---------------------------------------------------------------------------
class HostFrame : public IPlugFrame, public Linux::IRunLoop {
public:
  struct Timer {
    Linux::ITimerHandler* handler;
    uint64 intervalMs;
    uint64 nextDue;
  };
  struct Fd {
    Linux::IEventHandler* handler;
    int fd;
  };
  std::vector<Timer> timers;
  std::vector<Fd> fds;
  IPlugView* view = nullptr;
  Display* display = nullptr;
  Window window = 0;
  int resizeRequests = 0;

  static uint64 nowMs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64)ts.tv_sec * 1000 + (uint64)ts.tv_nsec / 1000000;
  }

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
    if (FUnknownPrivate::iidEqual(iid, IPlugFrame::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
      *obj = static_cast<IPlugFrame*>(this);
      return kResultOk;
    }
    if (FUnknownPrivate::iidEqual(iid, Linux::IRunLoop::iid)) {
      *obj = static_cast<Linux::IRunLoop*>(this);
      return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }

  tresult PLUGIN_API resizeView(IPlugView* v, ViewRect* rect) override {
    ++resizeRequests;
    if (display && window)
      XResizeWindow(display, window, (unsigned)rect->getWidth(), (unsigned)rect->getHeight());
    v->onSize(rect);
    return kResultTrue;
  }

  tresult PLUGIN_API registerEventHandler(Linux::IEventHandler* h, Linux::FileDescriptor fd) override {
    fds.push_back({h, fd});
    return kResultTrue;
  }
  tresult PLUGIN_API unregisterEventHandler(Linux::IEventHandler* h) override {
    fds.erase(std::remove_if(fds.begin(), fds.end(), [h](const Fd& f) { return f.handler == h; }), fds.end());
    return kResultTrue;
  }
  tresult PLUGIN_API registerTimer(Linux::ITimerHandler* h, Linux::TimerInterval ms) override {
    timers.push_back({h, ms, nowMs() + ms});
    return kResultTrue;
  }
  tresult PLUGIN_API unregisterTimer(Linux::ITimerHandler* h) override {
    timers.erase(std::remove_if(timers.begin(), timers.end(), [h](const Timer& t) { return t.handler == h; }),
                 timers.end());
    return kResultTrue;
  }

  // Runs the loop for `ms` milliseconds.
  void run(int ms) {
    const uint64 end = nowMs() + (uint64)ms;
    while (nowMs() < end) {
      std::vector<pollfd> pfds;
      for (const Fd& f : fds)
        pfds.push_back({f.fd, POLLIN, 0});
      uint64 next = end;
      for (const Timer& t : timers)
        next = std::min(next, t.nextDue);
      const uint64 now = nowMs();
      const int wait = next > now ? (int)(next - now) : 0;
      if (!pfds.empty())
        poll(pfds.data(), pfds.size(), wait);
      else if (wait > 0)
        poll(nullptr, 0, wait);
      for (size_t i = 0; i < pfds.size() && i < fds.size(); ++i)
        if (pfds[i].revents & POLLIN)
          fds[i].handler->onFDIsSet(fds[i].fd);
      const uint64 t = nowMs();
      std::vector<Timer> copy = timers;
      for (size_t i = 0; i < copy.size(); ++i) {
        if (t >= copy[i].nextDue) {
          copy[i].handler->onTimer();
          for (Timer& live : timers)
            if (live.handler == copy[i].handler)
              live.nextDue = t + live.intervalMs;
        }
      }
    }
  }
};

void sendMouse(Display* d, Window w, int type, int x, int y, unsigned button, unsigned state) {
  XEvent e;
  std::memset(&e, 0, sizeof(e));
  if (type == MotionNotify) {
    e.xmotion.type = MotionNotify;
    e.xmotion.display = d;
    e.xmotion.window = w;
    e.xmotion.x = x;
    e.xmotion.y = y;
    e.xmotion.state = state;
    XSendEvent(d, w, False, PointerMotionMask, &e);
  } else {
    e.xbutton.type = type;
    e.xbutton.display = d;
    e.xbutton.window = w;
    e.xbutton.x = x;
    e.xbutton.y = y;
    e.xbutton.button = button;
    e.xbutton.state = state;
    XSendEvent(d, w, False, type == ButtonPress ? ButtonPressMask : ButtonReleaseMask, &e);
  }
  XFlush(d);
}

bool saveWindow(Display* d, Window w, int width, int height, const char* path) {
  XImage* img = XGetImage(d, w, 0, 0, (unsigned)width, (unsigned)height, AllPlanes, ZPixmap);
  if (!img)
    return false;
  FILE* f = std::fopen(path, "wb");
  if (!f) {
    XDestroyImage(img);
    return false;
  }
  std::fprintf(f, "P6\n%d %d\n255\n", width, height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const unsigned long px = XGetPixel(img, x, y);
      const unsigned char rgb[3] = {(unsigned char)((px >> 16) & 255), (unsigned char)((px >> 8) & 255),
                                    (unsigned char)(px & 255)};
      std::fwrite(rgb, 1, 3, f);
    }
  }
  std::fclose(f);
  XDestroyImage(img);
  return true;
}

void runEditorTest(Plugin& p, const char* ppmPath, int dragX, int dragY, int dragDy) {
  std::printf("-- editor (X11) --\n");
  Display* display = XOpenDisplay(nullptr);
  if (!display) {
    check(false, "open X display (is DISPLAY set / Xvfb running?)");
    return;
  }

  HostFrame frame;
  frame.display = display;
  IPtr<IPlugView> view = owned(p.controller->createView(ViewType::kEditor));
  check(view != nullptr, "controller creates an editor view");
  if (!view)
    return;
  frame.view = view;
  check(view->isPlatformTypeSupported(kPlatformTypeX11EmbedWindowID) == kResultTrue, "view supports X11 embedding");

  ViewRect size;
  view->getSize(&size);
  check(size.getWidth() >= 640 && size.getHeight() >= 400, "view reports a sensible size");

  Window root = DefaultRootWindow(display);
  Window host = XCreateSimpleWindow(display, root, 0, 0, (unsigned)size.getWidth(), (unsigned)size.getHeight(), 0, 0, 0);
  frame.window = host;
  XMapWindow(display, host);
  XFlush(display);

  view->setFrame(&frame);
  check(view->attached((void*)(uintptr_t)host, kPlatformTypeX11EmbedWindowID) == kResultOk, "editor attaches");
  frame.run(1200);

  Window rootRet, parentRet, *children = nullptr;
  unsigned nChildren = 0;
  XQueryTree(display, host, &rootRet, &parentRet, &children, &nChildren);
  check(nChildren >= 1, "editor created a child window");
  Window child = nChildren ? children[0] : 0;
  if (children)
    XFree(children);

  if (child && ppmPath) {
    check(saveWindow(display, child, size.getWidth(), size.getHeight(), ppmPath), "screenshot written");
  }

  // Play a note while the editor is open so the scope and meters move.
  addNote(p, true, 60, 0.9f, 0);
  for (int i = 0; i < 40; ++i)
    processBlock(p);
  frame.run(400);

  if (child && dragX >= 0) {
    p.handler.edits.clear();
    p.handler.begins = p.handler.ends = 0;
    sendMouse(display, child, MotionNotify, dragX, dragY, 0, 0);
    frame.run(100);
    sendMouse(display, child, ButtonPress, dragX, dragY, Button1, 0);
    frame.run(100);
    const int steps = 8;
    for (int i = 1; i <= steps; ++i) {
      sendMouse(display, child, MotionNotify, dragX, dragY + dragDy * i / steps, 0, Button1Mask);
      frame.run(50);
    }
    sendMouse(display, child, ButtonRelease, dragX, dragY + dragDy, Button1, Button1Mask);
    frame.run(600);
    std::printf("drag produced %zu edits (begin=%d end=%d)\n", p.handler.edits.size(), p.handler.begins,
                p.handler.ends);
    check(!p.handler.edits.empty(), "dragging a knob sends parameter edits to the host");
    check(p.handler.begins >= 1 && p.handler.begins == p.handler.ends, "edit gestures are balanced");
    if (!p.handler.edits.empty()) {
      ParameterInfo info{};
      for (int32 i = 0; i < p.controller->getParameterCount(); ++i) {
        p.controller->getParameterInfo(i, info);
        if (info.id == p.handler.edits.back().id) {
          char name[128];
          for (int k = 0; k < 128; ++k) {
            name[k] = (char)info.title[k];
            if (!info.title[k]) break;
          }
          std::printf("edited parameter: %s -> %.3f\n", name, p.handler.edits.back().value);
        }
      }
    }
    if (ppmPath) {
      std::string after = std::string(ppmPath) + ".after.ppm";
      saveWindow(display, child, size.getWidth(), size.getHeight(), after.c_str());
    }
  }

  // Host-driven change shows up (no crash while the editor is open).
  const ParamID cutoff = idOf(p, "Filter Cutoff");
  p.controller->setParamNormalized(cutoff, 0.33);
  frame.run(200);

  // Resize round trip.
  ViewRect bigger(0, 0, size.getWidth() + 200, size.getHeight() + 100);
  view->checkSizeConstraint(&bigger);
  XResizeWindow(display, host, (unsigned)bigger.getWidth(), (unsigned)bigger.getHeight());
  view->onSize(&bigger);
  frame.run(300);
  ViewRect now;
  view->getSize(&now);
  check(now.getWidth() == bigger.getWidth() && now.getHeight() == bigger.getHeight(), "editor follows host resize");

  check(view->removed() == kResultOk, "editor detaches");
  view->setFrame(nullptr);
  check(frame.timers.empty() && frame.fds.empty(), "editor unregisters its run-loop handlers");

  // And again: open/close cycles must not leak or crash.
  view = owned(p.controller->createView(ViewType::kEditor));
  frame.view = view;
  view->setFrame(&frame);
  check(view->attached((void*)(uintptr_t)host, kPlatformTypeX11EmbedWindowID) == kResultOk, "editor re-attaches");
  frame.run(300);
  view->removed();
  view->setFrame(nullptr);
  view = nullptr;
  XDestroyWindow(display, host);
  XCloseDisplay(display);
}
#endif // __linux__

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: %s <plugin.vst3> [--editor out.ppm] [--drag x,y,dy]\n", argv[0]);
    return 2;
  }
  const std::string path = argv[1];
  const char* ppm = nullptr;
  bool editor = false;
  int dragX = -1, dragY = 0, dragDy = 0;
  for (int i = 2; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--editor")) {
      editor = true;
      if (i + 1 < argc && argv[i + 1][0] != '-')
        ppm = argv[++i];
    } else if (!std::strcmp(argv[i], "--drag") && i + 1 < argc) {
      std::sscanf(argv[++i], "%d,%d,%d", &dragX, &dragY, &dragDy);
    }
  }

  HostApplication host;
  Plugin plugin;
  if (!loadPlugin(plugin, path, host)) {
    std::printf("[FAIL] could not load and initialise %s\n", path.c_str());
    return 1;
  }
  check(true, "module loads and the component and controller initialise");
  std::printf("%d parameters\n", (int)plugin.controller->getParameterCount());

  runAudioTests(plugin);
  runStateTests(plugin);
#if defined(__linux__)
  if (editor)
    runEditorTest(plugin, ppm, dragX, dragY, dragDy);
#else
  (void)editor; (void)ppm; (void)dragX; (void)dragY; (void)dragDy;
#endif

  plugin.processor->setProcessing(false);
  plugin.component->setActive(false);
  plugin.provider.reset();
  plugin.module.reset();

  std::printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures, g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
