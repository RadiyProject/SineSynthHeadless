//------------------------------------------------------------------------
// Copyright(c) 2025 RadiyX.
//------------------------------------------------------------------------

#pragma once

#include "ADSR.h"
#include <chrono>

namespace radiyx {

struct SynthVoice
{
    bool active = false;
    int note = -1;
    float frequency = 0.f;
    float phase = 0.f;
    float deltaAngle = 0.f;
    uint64_t lastUsedAt;
    ADSR adsr;
};

//------------------------------------------------------------------------
} // namespace radiyx
