#include "processor.h"

#include "fpu.h"
#include "ids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>
#include <cmath>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace sdl2synth {

namespace {

constexpr uint32 kStateMagic = 0x53325359; // 'S2SY'
constexpr uint32 kStateVersion = 1;

float toDb(float linear) { return linear <= 1e-6f ? -120.0f : 20.0f * std::log10(linear); }

} // namespace

SynthProcessor::SynthProcessor() {
  setControllerClass(kControllerUID);
  processContextRequirements.needTempo();

  const std::vector<ParamDef>& defs = params();
  plain_.reset(new std::atomic<float>[defs.size()]);
  for (size_t i = 0; i < defs.size(); ++i)
    plain_[i].store(defs[i].def);
  arpTempoIndex_ = paramIndexByKey("arp.tempo");

  viz_ = createViz(vizToken_);
}

SynthProcessor::~SynthProcessor() {
  releaseViz(vizToken_);
  destroyEngine();
}

tresult PLUGIN_API SynthProcessor::initialize(FUnknown* context) {
  tresult result = AudioEffect::initialize(context);
  if (result != kResultOk)
    return result;

  addEventInput(STR16("MIDI In"), 1);
  addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

  createEngine(44100);
  return kResultOk;
}

tresult PLUGIN_API SynthProcessor::terminate() {
  destroyEngine();
  return AudioEffect::terminate();
}

void SynthProcessor::createEngine(int sampleRate) {
  destroyEngine();

  std::unique_ptr<Synth> synth(new Synth);
  if (!synth_core_init(synth.get(), sampleRate, kVoices))
    return;
  synth->scope = viz_ ? &viz_->scope : nullptr;
  synth_ = std::move(synth);
  sampleRate_ = sampleRate;
  applyAllParams();
}

void SynthProcessor::destroyEngine() {
  if (synth_) {
    synth_core_shutdown(synth_.get());
    synth_.reset();
  }
  sampleRate_ = 0;
}

void SynthProcessor::applyParam(int index, float plain) {
  const ParamDef& def = params()[(size_t)index];
  plain = clampPlain(def, plain);
  plain_[(size_t)index].store(plain);
  if (synth_)
    applyPlain(*synth_, def, plain);
}

void SynthProcessor::applyAllParams() {
  const std::vector<ParamDef>& defs = params();
  for (size_t i = 0; i < defs.size(); ++i)
    applyParam((int)i, plain_[i].load());
}

tresult PLUGIN_API SynthProcessor::setActive(TBool state) {
  if (synth_) {
    // Start from silence whenever processing is (re)started or stopped so no
    // hanging notes or old delay tails leak into the next play-back.
    synth_all_notes_off(synth_.get());
    synth_reset(synth_.get());
  }
  return AudioEffect::setActive(state);
}

tresult PLUGIN_API SynthProcessor::setupProcessing(ProcessSetup& setup) {
  tresult result = AudioEffect::setupProcessing(setup);
  if (result != kResultOk)
    return result;

  const int sampleRate = std::max(8000, (int)std::lround(setup.sampleRate));
  if (!synth_ || sampleRate != sampleRate_)
    createEngine(sampleRate);
  return synth_ ? kResultOk : kResultFalse;
}

tresult PLUGIN_API SynthProcessor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                      SpeakerArrangement* outputs, int32 numOuts) {
  // Instrument: no audio inputs, a single stereo output.
  if (numIns == 0 && numOuts == 1 && outputs[0] == SpeakerArr::kStereo)
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
  return kResultFalse;
}

tresult PLUGIN_API SynthProcessor::canProcessSampleSize(int32 symbolicSampleSize) {
  return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

uint32 PLUGIN_API SynthProcessor::getTailSamples() {
  // Delay and reverb feedback can ring for a long time.
  return kInfiniteTail;
}

void SynthProcessor::handleEvent(const Event& e) {
  switch (e.type) {
    case Event::kNoteOnEvent:
      synth_midi_note_on(synth_.get(), e.noteOn.pitch, e.noteOn.velocity);
      break;
    case Event::kNoteOffEvent:
      synth_midi_note_off(synth_.get(), e.noteOff.pitch);
      break;
    default:
      break;
  }
}

void SynthProcessor::renderSegment(float* left, float* right, int frames) {
  while (frames > 0) {
    const int n = std::min(frames, kRenderBlock);
    synth_render(synth_.get(), scratch_, n);
    for (int i = 0; i < n; ++i) {
      left[i] = scratch_[2 * i];
      right[i] = scratch_[2 * i + 1];
    }
    left += n;
    right += n;
    frames -= n;
  }
}

tresult PLUGIN_API SynthProcessor::process(ProcessData& data) {
  if (!synth_)
    return kResultFalse;
  ScopedFlushDenormals flushDenormals;

  // --- Parameter changes: apply the last value of each queue --------------
  if (data.inputParameterChanges) {
    const int32 count = data.inputParameterChanges->getParameterCount();
    for (int32 i = 0; i < count; ++i) {
      IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
      if (!queue)
        continue;
      const int index = paramIndex(queue->getParameterId());
      const int32 points = queue->getPointCount();
      if (index < 0 || points <= 0)
        continue;
      int32 sampleOffset = 0;
      ParamValue value = 0.0;
      if (queue->getPoint(points - 1, sampleOffset, value) == kResultTrue)
        applyParam(index, (float)normalizedToPlain(params()[(size_t)index], value));
    }
  }

  // --- Tempo: follow the host when asked to ---------------------------------
  float tempo = plain_[(size_t)arpTempoIndex_].load();
  if (synth_->host_sync && data.processContext &&
      (data.processContext->state & ProcessContext::kTempoValid) && data.processContext->tempo > 0.0)
    tempo = (float)std::min(std::max(data.processContext->tempo, 20.0), 400.0);
  synth_->arp.tempo = tempo;
  synth_set_bpm(synth_.get(), tempo);

  // --- Audio ---------------------------------------------------------------------
  const int32 numSamples = data.numSamples;
  if (numSamples <= 0 || data.numOutputs < 1 || data.outputs[0].numChannels < 1 ||
      !data.outputs[0].channelBuffers32)
    return kResultOk;

  float* left = data.outputs[0].channelBuffers32[0];
  float* right = data.outputs[0].numChannels > 1 ? data.outputs[0].channelBuffers32[1] : nullptr;
  if (!right) {
    // Not expected (we only accept stereo) but never crash on odd hosts.
    std::fill(left, left + numSamples, 0.0f);
    return kResultOk;
  }

  // Split the block at event positions so notes start sample-accurately.
  IEventList* events = data.inputEvents;
  const int32 eventCount = events ? events->getEventCount() : 0;
  int32 position = 0;
  for (int32 i = 0; i < eventCount; ++i) {
    Event e;
    if (events->getEvent(i, e) != kResultOk)
      continue;
    const int32 offset = std::min(std::max(e.sampleOffset, 0), numSamples);
    if (offset > position) {
      renderSegment(left + position, right + position, offset - position);
      position = offset;
    }
    handleEvent(e);
  }
  if (position < numSamples)
    renderSegment(left + position, right + position, numSamples - position);

  data.outputs[0].silenceFlags = 0;

  // --- Meters for the editor -----------------------------------------------------
  if (viz_) {
    float peak = 0.0f, sum = 0.0f;
    for (int32 i = 0; i < numSamples; ++i) {
      const float l = left[i], r = right[i];
      peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
      sum += 0.5f * (l * l + r * r);
    }
    viz_->peakDb.store(toDb(peak));
    viz_->rmsDb.store(toDb(std::sqrt(sum / (float)numSamples)));
    viz_->blocks.fetch_add(1, std::memory_order_relaxed);
  }
  return kResultOk;
}

tresult PLUGIN_API SynthProcessor::getState(IBStream* state) {
  IBStreamer streamer(state, kLittleEndian);
  const std::vector<ParamDef>& defs = params();
  if (!streamer.writeInt32u(kStateMagic) || !streamer.writeInt32u(kStateVersion) ||
      !streamer.writeInt32u((uint32)defs.size()))
    return kResultFalse;
  for (size_t i = 0; i < defs.size(); ++i) {
    if (!streamer.writeInt32u(defs[i].id) || !streamer.writeFloat(plain_[i].load()))
      return kResultFalse;
  }
  return kResultOk;
}

tresult PLUGIN_API SynthProcessor::setState(IBStream* state) {
  IBStreamer streamer(state, kLittleEndian);
  uint32 magic = 0, version = 0, count = 0;
  if (!streamer.readInt32u(magic) || magic != kStateMagic)
    return kResultFalse;
  if (!streamer.readInt32u(version) || version > kStateVersion)
    return kResultFalse;
  if (!streamer.readInt32u(count))
    return kResultFalse;

  // Parameters missing from the state (older versions) keep their defaults.
  const std::vector<ParamDef>& defs = params();
  for (size_t i = 0; i < defs.size(); ++i)
    plain_[i].store(defs[i].def);

  for (uint32 n = 0; n < count; ++n) {
    uint32 id = 0;
    float value = 0.0f;
    if (!streamer.readInt32u(id) || !streamer.readFloat(value))
      return kResultFalse;
    const int index = paramIndex(id);
    if (index >= 0 && std::isfinite(value))
      plain_[(size_t)index].store(clampPlain(defs[(size_t)index], value));
  }
  applyAllParams();
  return kResultOk;
}

tresult PLUGIN_API SynthProcessor::connect(IConnectionPoint* other) {
  tresult result = AudioEffect::connect(other);
  if (result == kResultOk)
    sendVizMessage();
  return result;
}

void SynthProcessor::sendVizMessage() {
  if (!vizToken_)
    return;
  if (IPtr<IMessage> message = owned(allocateMessage())) {
    message->setMessageID(SDL2SYNTH_VIZ_MESSAGE);
    message->getAttributes()->setInt("token", (int64)vizToken_);
    sendMessage(message);
  }
}

} // namespace sdl2synth
