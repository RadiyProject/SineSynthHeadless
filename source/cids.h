//------------------------------------------------------------------------
// Copyright(c) 2025 RadiyX.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace radiyx {
//------------------------------------------------------------------------
static const Steinberg::FUID kSineSynthProcessorUID (0x072865FD, 0xCC115CBD, 0xB698A7D6, 0x1F3767D2);
static const Steinberg::FUID kSineSynthControllerUID (0x282CC5A4, 0x78F55E27, 0xB3AF8003, 0x2143F720);

#define SineSynthVST3Category "Instrument"

//------------------------------------------------------------------------
} // namespace radiyx
