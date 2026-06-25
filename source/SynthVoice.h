//------------------------------------------------------------------------
// Copyright(c) 2025 RadiyX.
//------------------------------------------------------------------------

#pragma once

#include "ADSR.h"
#include <chrono>
#include "base/source/fstreamer.h"

namespace radiyx {

struct SynthVoice
{
    bool active = false;
    int note = -1;
    float frequency = 0.f;
    float phase = 0.f;
    float deltaAngle = 0.f;
    float velocity = 1.f;
    uint64_t lastUsedAt = 0;
    ADSR adsr;

    bool WriteState(Steinberg::IBStreamer& streamer) const
    {
        streamer.writeInt32(active ? 1 : 0);
        streamer.writeInt32(static_cast<Steinberg::int32>(note));
        streamer.writeFloat(frequency);
        streamer.writeFloat(phase);
        streamer.writeFloat(deltaAngle);
        streamer.writeInt64(static_cast<Steinberg::int64>(lastUsedAt));

        return adsr.WriteState(streamer);
    }

    bool ReadState(Steinberg::IBStreamer& streamer)
    {
        Steinberg::int32 activeValue = 0;
        Steinberg::int32 noteValue = -1;
        Steinberg::int64 lastUsedValue = 0;

        if (!streamer.readInt32(activeValue)) return false;
        if (!streamer.readInt32(noteValue)) return false;
        if (!streamer.readFloat(frequency)) return false;
        if (!streamer.readFloat(phase)) return false;
        if (!streamer.readFloat(deltaAngle)) return false;
        if (!streamer.readInt64(lastUsedValue)) return false;

        if (!adsr.ReadState(streamer)) return false;

        active = activeValue != 0;
        note = static_cast<int>(noteValue);
        velocity = 1.f;
        lastUsedAt = static_cast<uint64_t>(lastUsedValue);

        return true;
    }
};

//------------------------------------------------------------------------
} // namespace radiyx
