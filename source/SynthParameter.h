//------------------------------------------------------------------------
// Copyright(c) 2025 RadiyX.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/vst/vsttypes.h"

namespace radiyx {

class SynthParameter
{
    public:
        enum Value : Steinberg::Vst::ParamID
        {
            Sine = 100,
            Saw,
            Square,
            Triangle,

            Attack = 200,
            Decay,
            Sustain,
            Release
        };

        SynthParameter() = default;
        constexpr SynthParameter(Value parameter): value(parameter) { }

        constexpr bool operator==(SynthParameter parameter) const { return value == parameter.value; }
        constexpr bool operator!=(SynthParameter parameter) const { return value != parameter.value; }

        static constexpr float DEFAULT_SINE = 0.8f;
        static constexpr float DEFAULT_SAW = 0.4f;
        static constexpr float DEFAULT_SQUARE = 0.;
        static constexpr float DEFAULT_TRIANGLE = 0.;

        static constexpr float DEFAULT_ATTACK = 0.1f;
        static constexpr float DEFAULT_DECAY = 0.1f;
        static constexpr float DEFAULT_SUSTAIN = 0.8f;
        static constexpr float DEFAULT_RELEASE = 0.3f;

    private:
        Value value;
};

//------------------------------------------------------------------------
} // namespace radiyx
