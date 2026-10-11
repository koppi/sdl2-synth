#pragma once

#include "pluginterfaces/gui/iplugview.h"

namespace sdl2synth {

class SynthController;

// Creates the plug-in's editor view (owned by the caller, reference counted).
Steinberg::IPlugView* createSynthEditorView(SynthController* controller);

} // namespace sdl2synth
