#pragma once

#include "params.h"
#include "viz.h"

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"

#include <atomic>
#include <memory>
#include <vector>

namespace sdl2synth {

// Audio side of the plug-in: an instrument with one stereo output and one
// MIDI/event input, driving the shared synth engine.
class SynthProcessor : public Steinberg::Vst::AudioEffect {
public:
  SynthProcessor();
  ~SynthProcessor() override;

  static Steinberg::FUnknown* createInstance(void*) {
    return static_cast<Steinberg::Vst::IAudioProcessor*>(new SynthProcessor);
  }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
                                                   Steinberg::Vst::SpeakerArrangement* outputs,
                                                   Steinberg::int32 numOuts) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
  Steinberg::uint32 PLUGIN_API getTailSamples() SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API connect(Steinberg::Vst::IConnectionPoint* other) SMTG_OVERRIDE;

private:
  static constexpr int kRenderBlock = 512; // frames rendered per synth_render() call
  static constexpr int kVoices = 16;

  void createEngine(int sampleRate);
  void destroyEngine();
  void applyParam(int index, float plain);
  void applyAllParams();
  void handleEvent(const Steinberg::Vst::Event& e);
  void renderSegment(float* left, float* right, int frames);
  void sendVizMessage();

  std::unique_ptr<Synth> synth_;
  int sampleRate_ = 0;

  // Current plain value of every parameter, indexed like params(). Written on
  // the audio thread, read when the host asks for the state.
  std::unique_ptr<std::atomic<float>[]> plain_;
  int arpTempoIndex_ = -1;

  std::shared_ptr<VizData> viz_;
  uint64_t vizToken_ = 0;

  float scratch_[kRenderBlock * 2];
};

} // namespace sdl2synth
