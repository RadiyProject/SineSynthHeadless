#pragma once

namespace radiyx {

class ADSR
{
    public:
        enum class State { Idle, Attack, Decay, Sustain, Release };

        ADSR()
            : sampleRate(44100.0),
            attackTime(0.01f), decayTime(0.1f),
            sustainLevel(0.8f), releaseTime(0.3f),
            currentState(State::Idle), currentLevel(0.0f)
        {}

        ADSR& SetSampleRate(double sampleRate);
        
        ADSR& SetAttack(float attackTime);
        ADSR& SetDecay(float decayTime);
        ADSR& SetSustain(float sustainLevel);
        ADSR& SetRelease(float releaseTime);

        float GetAttack();
        float GetDecay();
        float GetSustain();
        float GetRelease();

        ADSR& NoteOn();
        ADSR& NoteOff();

        float GetCurrentVolumeModifier();

        bool IsActive() const;
        bool IsInRelease() const;

    private:
        void RecalculateSteps();

        double sampleRate;
        float attackTime, decayTime, sustainLevel, releaseTime;
        float attackStep = 0, decayStep = 0, releaseStep = 0;
        float currentLevel;
        State currentState;
};

} // namespace radiyx
