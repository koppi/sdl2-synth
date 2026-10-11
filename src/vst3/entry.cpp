#include "controller.h"
#include "ids.h"
#include "processor.h"

#include "public.sdk/source/main/pluginfactory.h"

#ifndef SDL2SYNTH_VERSION_STR
#define SDL2SYNTH_VERSION_STR "0.1.0"
#endif

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF(SDL2SYNTH_VENDOR, SDL2SYNTH_URL, "mailto:" SDL2SYNTH_EMAIL)

DEF_CLASS2(INLINE_UID_FROM_FUID(sdl2synth::kProcessorUID),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           SDL2SYNTH_NAME,
           Vst::kDistributable,
           Vst::PlugType::kInstrumentSynth,
           SDL2SYNTH_VERSION_STR,
           kVstVersionString,
           sdl2synth::SynthProcessor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(sdl2synth::kControllerUID),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           SDL2SYNTH_NAME " Controller",
           0,
           "",
           SDL2SYNTH_VERSION_STR,
           kVstVersionString,
           sdl2synth::SynthController::createInstance)

END_FACTORY
