#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

// Identity of the plug-in. The class IDs below are part of the plug-in's public
// interface: hosts use them to find saved projects' instruments. Never change
// them once a release has been published.

#define SDL2SYNTH_VENDOR "koppi"
#define SDL2SYNTH_URL "https://github.com/koppi/sdl2-synth"
#define SDL2SYNTH_EMAIL "jakob.flierl@gmail.com"
#define SDL2SYNTH_NAME "SDL2 Synth"

namespace sdl2synth {

static const Steinberg::FUID kProcessorUID (0x38BFED43, 0xF872448E, 0xBB7A50EE, 0x4466B7C2);
static const Steinberg::FUID kControllerUID (0x23F036B2, 0xA29740E1, 0xA2E0D213, 0xA323C007);

// Name of the IMessage the processor sends to hand its visualisation data to
// the controller (see viz.h).
#define SDL2SYNTH_VIZ_MESSAGE "sdl2synth.viz"

} // namespace sdl2synth
