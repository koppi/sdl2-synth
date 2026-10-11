#pragma once

#include "params.h"
#include "viz.h"

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#include <memory>

namespace sdl2synth {

// Host-facing parameter list, MIDI controller assignments and the editor.
class SynthController : public Steinberg::Vst::EditControllerEx1, public Steinberg::Vst::IMidiMapping {
public:
  static Steinberg::FUnknown* createInstance(void*) {
    return static_cast<Steinberg::Vst::IEditController*>(new SynthController);
  }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

  // IMidiMapping
  Steinberg::tresult PLUGIN_API getMidiControllerAssignment(Steinberg::int32 busIndex, Steinberg::int16 channel,
                                                            Steinberg::Vst::CtrlNumber midiControllerNumber,
                                                            Steinberg::Vst::ParamID& id) SMTG_OVERRIDE;

  // Used by the editor.
  std::shared_ptr<VizData> viz() const { return viz_; }

  // Make the base-class edit hooks callable from the editor.
  using EditController::beginEdit;
  using EditController::endEdit;
  using EditController::performEdit;

  DEFINE_INTERFACES
    DEF_INTERFACE(Steinberg::Vst::IMidiMapping)
  END_DEFINE_INTERFACES(EditControllerEx1)
  REFCOUNT_METHODS(EditControllerEx1)

private:
  std::shared_ptr<VizData> viz_;
};

} // namespace sdl2synth
