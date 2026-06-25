//------------------------------------------------------------------------
// Copyright(c) 2025 RadiyX.
//------------------------------------------------------------------------

#pragma once

#include "SynthParameter.h"
#include "ADSR.h"
#include <vector>
#include "SynthVoice.h"
#include "base/source/fstreamer.h"

namespace radiyx {

class Synth
{
    public:
        Synth(size_t polyphony = 128) : voices(polyphony) {}

        Synth& SetSine(float value);
        Synth& SetSaw(float value);
        Synth& SetSquare(float value);
        Synth& SetTriangle(float value);

        Synth& NoteOn(int note, float velocity = 1.f);
        Synth& NoteOff(int note);
        Synth& SetVolume(float volume);
        Synth& SetSampleRate(double rate);
        Synth& SetTune(float tune);

        Synth& ResetRuntime();
        Synth& ResetToDefaults();

        bool WriteState(Steinberg::IBStreamer& streamer) const;
        bool ReadState(Steinberg::IBStreamer& streamer);

        Synth& RebindRuntimeToSampleRate();

        ADSR adsr;

        float GetSine();
        float GetSaw();
        float GetSquare();
        float GetTriangle();

        float GetVolume();
        double GetSampleRate();

        void GetOutputSignal(float* leftChannel, float* rightChannel, int samplesCount);

    protected:
        static constexpr float PI = 3.14159265358979323846f;
        static constexpr float PI_DOUBLED = 2.0f * PI;
        static constexpr int SEMITONES_COUNT = 12;

    private:
        float sine = SynthParameter::DEFAULT_SINE;
        float saw = SynthParameter::DEFAULT_SAW;
        float square = SynthParameter::DEFAULT_SQUARE;
        float triangle = SynthParameter::DEFAULT_TRIANGLE;

        float frequency = 0.f;
        float volume = 0.6f;
        float tune = 440.f;
        double sampleRate = 44100.0;
        std::vector<SynthVoice> voices;

        SynthVoice* FindFreeVoice();
        SynthVoice* FindVoice(int note);
};

//------------------------------------------------------------------------
} // namespace radiyx
