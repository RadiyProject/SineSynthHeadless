//------------------------------------------------------------------------
// Copyright(c) 2025 RadiyX.
//------------------------------------------------------------------------

#include "Synth.h"
#include "cids.h"
#include <ctime>
#include "Log.h"
#include <cmath>
#include <algorithm>

namespace radiyx {
    Synth& Synth::SetSine(float value)
    {
        this->sine = value;

        return *this;
    }
    Synth& Synth::SetSaw(float value)
    {
        this->saw = value;

        return *this;
    }
    Synth& Synth::SetSquare(float value)
    {
        this->square = value;

        return *this;
    }
    Synth& Synth::SetTriangle(float value)
    {
        this->triangle = value;

        return *this;
    }

    Synth& Synth::NoteOn(int note, float velocity)
    {
        velocity = std::clamp(velocity, 0.0f, 1.0f);

        if (auto* voice = this->FindVoice(note))
        {
            voice->lastUsedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();

            voice->adsr.SetAttack(this->adsr.GetAttack());
            voice->adsr.SetDecay(this->adsr.GetDecay());
            voice->adsr.SetSustain(this->adsr.GetSustain());
            voice->adsr.SetRelease(this->adsr.GetRelease());

            voice->adsr.NoteOn();
            voice->velocity = velocity;
            voice->active = true;

            return *this;
        }

        if (auto* voice = this->FindFreeVoice())
        {
            Log::Instance().Push("Free voice activation");
            voice->note = note;
            voice->velocity = velocity;
            voice->frequency = this->tune * pow(2.f, float(note - 69) / float(SEMITONES_COUNT));
            voice->deltaAngle = Synth::PI_DOUBLED * voice->frequency / this->GetSampleRate();
            voice->adsr.SetSampleRate(this->GetSampleRate());
            voice->lastUsedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();

            voice->adsr.SetAttack(this->adsr.GetAttack());
            voice->adsr.SetDecay(this->adsr.GetDecay());
            voice->adsr.SetSustain(this->adsr.GetSustain());
            voice->adsr.SetRelease(this->adsr.GetRelease());

            voice->adsr.NoteOn();
            voice->active = true;

            Log::Instance().Push("Free voice activated");
        }

        return *this;
    }

    Synth& Synth::NoteOff(int note)
    {
        for (auto& voice : voices)
            if (voice.active && voice.note == note)
                voice.adsr.NoteOff();

        return *this;
    }

    Synth& Synth::SetVolume(float volume)
    {
        this->volume = volume;

        return *this;
    }

    Synth &Synth::SetSampleRate(double rate)
    {
        this->sampleRate = rate;

        return *this;
    }

    Synth &Synth::SetTune(float tune)
    {
        this->tune = tune;

        return *this;
    }

    float Synth::GetSine()
    {
        return this->sine;
    }
    float Synth::GetSaw()
    {
        return this->saw;
    }
    float Synth::GetSquare()
    {
        return this->square;
    }
    float Synth::GetTriangle()
    {
        return this->triangle;
    }

    float Synth::GetVolume()
    {
        return this->volume;
    }

    double Synth::GetSampleRate()
    {
        return this->sampleRate;
    }

    void Synth::GetOutputSignal(float *leftChannel, float *rightChannel, int samplesCount)
    {
        float totalWeight = this->GetSine() + this->GetSaw() + this->GetSquare() + this->GetTriangle();
        float normalizationFactor = totalWeight > 1.0f ? (1.0f / totalWeight) : 1.0f;
        for (int sample = 0; sample < samplesCount; sample++)
        {
            float sampleValue = 0.f;
            float gainSum = 0.0f;
            for (auto& voice : voices)
            {
                if (!voice.active)
                    continue;

                float envelope = voice.adsr.GetCurrentVolumeModifier();

                float voiceSample = 0.0f;
                voiceSample += normalizationFactor * this->GetSine() * sin(voice.phase);
                voiceSample += normalizationFactor * this->GetSaw() * (2.0f * (voice.phase / Synth::PI_DOUBLED) - 1.0f);
                voiceSample += normalizationFactor * this->GetSquare() * ((voice.phase < Synth::PI) ? 1.0f : -1.0f);
                voiceSample += normalizationFactor * this->GetTriangle() * (2.0f * fabs(2.0f * (voice.phase / Synth::PI_DOUBLED) - 1.0f) - 1.0f);

                voiceSample *= this->GetVolume() * envelope * voice.velocity;
                sampleValue += voiceSample;
                gainSum += envelope;

                voice.phase += voice.deltaAngle;
                if (voice.phase >= PI_DOUBLED)
                    voice.phase -= PI_DOUBLED;

                if (!voice.adsr.IsActive())
                    voice.active = false;

            }

            if (gainSum > 1.0f)
                sampleValue /= gainSum;

            leftChannel[sample] = sampleValue;
            rightChannel[sample] = sampleValue;
        }
    }

    SynthVoice* Synth::FindFreeVoice()
    {
        for (auto& voice : voices)
            if (!voice.active)
                return &voice;

        for (auto& voice : voices)
            if (voice.adsr.IsInRelease())
                return &voice;

        int oldVoiceIndex = 0;
        int voicesCount = size(voices);
        if (voicesCount > 1) {
            for (int voiceIndex = 1; voiceIndex < size(voices); voiceIndex++)
                if (voices[voiceIndex].lastUsedAt < voices[oldVoiceIndex].lastUsedAt)
                    oldVoiceIndex = voiceIndex;
        }

        return &voices[oldVoiceIndex];
    }

    SynthVoice* Synth::FindVoice(int note)
    {
        for (auto& voice : voices)
        {
            if (voice.active && voice.note == note)
                return &voice;
        }
        return nullptr;
    }

    Synth& Synth::ResetRuntime()
    {
        frequency = 0.f;

        for (auto& voice : voices)
        {
            voice.active = false;
            voice.note = -1;
            voice.frequency = 0.f;
            voice.phase = 0.f;
            voice.deltaAngle = 0.f;
            voice.velocity = 1.f;
            voice.lastUsedAt = 0;

            voice.adsr.ResetRuntime();
            voice.adsr.SetSampleRate(this->GetSampleRate());
            voice.adsr.SetAttack(this->adsr.GetAttack());
            voice.adsr.SetDecay(this->adsr.GetDecay());
            voice.adsr.SetSustain(this->adsr.GetSustain());
            voice.adsr.SetRelease(this->adsr.GetRelease());
        }

        return *this;
    }

    Synth& Synth::ResetToDefaults()
    {
        sine = SynthParameter::DEFAULT_SINE;
        saw = SynthParameter::DEFAULT_SAW;
        square = SynthParameter::DEFAULT_SQUARE;
        triangle = SynthParameter::DEFAULT_TRIANGLE;

        volume = 0.6f;
        tune = 440.f;
        frequency = 0.f;

        adsr.SetSampleRate(this->GetSampleRate());
        adsr.SetAttack(SynthParameter::DEFAULT_ATTACK);
        adsr.SetDecay(SynthParameter::DEFAULT_DECAY);
        adsr.SetSustain(SynthParameter::DEFAULT_SUSTAIN);
        adsr.SetRelease(SynthParameter::DEFAULT_RELEASE);
        adsr.ResetRuntime();

        ResetRuntime();

        return *this;
    }

    bool Synth::WriteState(Steinberg::IBStreamer& streamer) const
    {
        streamer.writeFloat(sine);
        streamer.writeFloat(saw);
        streamer.writeFloat(square);
        streamer.writeFloat(triangle);

        streamer.writeFloat(volume);
        streamer.writeFloat(tune);
        streamer.writeFloat(frequency);
        streamer.writeDouble(sampleRate);

        if (!adsr.WriteState(streamer)) {
            return false;
        }

        streamer.writeInt32(static_cast<Steinberg::int32>(voices.size()));

        for (const auto& voice : voices)
        {
            if (!voice.WriteState(streamer)) {
                return false;
            }
        }

        return true;
    }

    bool Synth::ReadState(Steinberg::IBStreamer& streamer)
    {
        ResetToDefaults();

        if (!streamer.readFloat(sine)) return false;
        if (!streamer.readFloat(saw)) return false;
        if (!streamer.readFloat(square)) return false;
        if (!streamer.readFloat(triangle)) return false;

        if (!streamer.readFloat(volume)) return false;
        if (!streamer.readFloat(tune)) return false;
        if (!streamer.readFloat(frequency)) return false;
        if (!streamer.readDouble(sampleRate)) return false;

        if (!adsr.ReadState(streamer)) return false;

        Steinberg::int32 voiceCount = 0;
        if (!streamer.readInt32(voiceCount)) return false;

        if (voiceCount < 0 || voiceCount > 256) {
            return false;
        }

        voices.resize(static_cast<size_t>(voiceCount));

        for (auto& voice : voices)
        {
            if (!voice.ReadState(streamer)) {
                return false;
            }
        }

        return true;
    }

    Synth& Synth::RebindRuntimeToSampleRate()
    {
        for (auto& voice : voices)
        {
            voice.adsr.SetSampleRate(this->GetSampleRate());

            if (voice.active && voice.frequency > 0.0f)
            {
                voice.deltaAngle = Synth::PI_DOUBLED * voice.frequency / this->GetSampleRate();
            }
        }

        return *this;
    }

//------------------------------------------------------------------------
} // namespace radiyx
