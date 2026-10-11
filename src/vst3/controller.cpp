#include "controller.h"

#include "editor.h"
#include "ids.h"

#include "base/source/fstreamer.h"
#include "base/source/fstring.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#include <algorithm>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace sdl2synth {

namespace {

constexpr uint32 kStateMagic = 0x53325359; // 'S2SY', must match the processor

// Unit IDs are 1-based (0 is the root unit).
UnitID unitIdFor(int unitIndex) { return (UnitID)(unitIndex + 1); }

// A parameter whose conversions come from the shared parameter table.
class SynthParameter : public Parameter {
public:
  explicit SynthParameter(const ParamDef& def) : def_(def) {
    UString title(info.title, str16BufferSize(String128));
    title.fromAscii(def.title.c_str());
    UString shortTitle(info.shortTitle, str16BufferSize(String128));
    shortTitle.fromAscii(def.shortTitle.c_str());
    UString units(info.units, str16BufferSize(String128));
    units.fromAscii(def.units.c_str());

    info.id = def.id;
    info.stepCount = stepCount(def);
    info.defaultNormalizedValue = plainToNormalized(def, def.def);
    info.unitId = unitIdFor(def.unit);
    info.flags = ParameterInfo::kCanAutomate;
    if (def.kind == ParamKind::Choice)
      info.flags |= ParameterInfo::kIsList;
    valueNormalized = info.defaultNormalizedValue;
  }

  void toString(ParamValue normalized, String128 string) const SMTG_OVERRIDE {
    UString(string, str16BufferSize(String128))
        .fromAscii(formatValue(def_, normalizedToPlain(def_, normalized)).c_str());
  }

  bool fromString(const TChar* string, ParamValue& normalized) const SMTG_OVERRIDE {
    String text(string);
    text.toMultiByte(kCP_Utf8);
    double plain = 0.0;
    if (!parseValue(def_, text.text8(), plain))
      return false;
    normalized = plainToNormalized(def_, plain);
    return true;
  }

  ParamValue toPlain(ParamValue normalized) const SMTG_OVERRIDE { return normalizedToPlain(def_, normalized); }
  ParamValue toNormalized(ParamValue plain) const SMTG_OVERRIDE { return plainToNormalized(def_, plain); }

private:
  const ParamDef& def_;
};

} // namespace

tresult PLUGIN_API SynthController::initialize(FUnknown* context) {
  tresult result = EditControllerEx1::initialize(context);
  if (result != kResultOk)
    return result;

  const std::vector<std::string>& names = unitNames();
  for (size_t i = 0; i < names.size(); ++i) {
    UString128 name(names[i].c_str());
    addUnit(new Unit(name, unitIdFor((int)i), kRootUnitId));
  }
  for (const ParamDef& def : params())
    parameters.addParameter(new SynthParameter(def));
  return kResultOk;
}

tresult PLUGIN_API SynthController::terminate() {
  viz_.reset();
  return EditControllerEx1::terminate();
}

tresult PLUGIN_API SynthController::setComponentState(IBStream* state) {
  if (!state)
    return kResultFalse;
  IBStreamer streamer(state, kLittleEndian);
  uint32 magic = 0, version = 0, count = 0;
  if (!streamer.readInt32u(magic) || magic != kStateMagic || !streamer.readInt32u(version) ||
      !streamer.readInt32u(count))
    return kResultFalse;

  for (uint32 n = 0; n < count; ++n) {
    uint32 id = 0;
    float value = 0.0f;
    if (!streamer.readInt32u(id) || !streamer.readFloat(value))
      return kResultFalse;
    const int index = paramIndex(id);
    if (index >= 0)
      setParamNormalized(id, plainToNormalized(params()[(size_t)index], value));
  }
  return kResultOk;
}

IPlugView* PLUGIN_API SynthController::createView(FIDString name) {
  if (FIDStringsEqual(name, ViewType::kEditor))
    return createSynthEditorView(this);
  return nullptr;
}

tresult PLUGIN_API SynthController::notify(IMessage* message) {
  if (message && FIDStringsEqual(message->getMessageID(), SDL2SYNTH_VIZ_MESSAGE)) {
    int64 token = 0;
    if (message->getAttributes()->getInt("token", token) == kResultOk)
      viz_ = findViz((uint64_t)token);
    return kResultOk;
  }
  return EditControllerEx1::notify(message);
}

tresult PLUGIN_API SynthController::getMidiControllerAssignment(int32 busIndex, int16 /*channel*/,
                                                                CtrlNumber midiControllerNumber, ParamID& id) {
  if (busIndex != 0)
    return kResultFalse;

  const char* key = nullptr;
  if (midiControllerNumber == kPitchBend) {
    key = kPitchBendKey;
  } else {
    for (const CcAssignment& a : midiCcAssignments()) {
      if (a.cc == midiControllerNumber) {
        key = a.key;
        break;
      }
    }
  }
  if (!key)
    return kResultFalse;
  const int index = paramIndexByKey(key);
  if (index < 0)
    return kResultFalse;
  id = params()[(size_t)index].id;
  return kResultTrue;
}

} // namespace sdl2synth
