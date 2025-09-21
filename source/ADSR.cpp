//------------------------------------------------------------------------
// Copyright(c) 2025 RadiyX.
//------------------------------------------------------------------------

#include "ADSR.h"
#include <cmath>

namespace radiyx {

    ADSR& ADSR::SetSampleRate(double sampleRate)
    {
        this->sampleRate = sampleRate; 
        this->RecalculateSteps();

        return *this;
    }

    ADSR& ADSR::SetAttack(float attackTime)  
    { 
        this->attackTime = attackTime * 0.1f;
        if (this->attackTime <= 0)
            this->attackTime = 0.002f;

        this->RecalculateSteps();

        return *this;
    }

    ADSR& ADSR::SetDecay(float decayTime)   
    { 
        this->decayTime = decayTime; 
        RecalculateSteps();

        return *this;
    }

    ADSR& ADSR::SetSustain(float sustainLevel) 
    { 
        this->sustainLevel = sustainLevel;

        return *this;
    }

    ADSR& ADSR::SetRelease(float releaseTime) 
    { 
        this->releaseTime = releaseTime; 
        RecalculateSteps();

        return *this;
    }

    float ADSR::GetAttack()
    {
        return this->attackTime * 10.f;
    }

    float ADSR::GetDecay()
    {
        return this->decayTime;
    }

    float ADSR::GetSustain()
    {
        return this->sustainLevel;
    }

    float ADSR::GetRelease()
    {
        return this->releaseTime;
    }

    ADSR& ADSR::NoteOn()  
    { 
        this->currentState = State::Attack;
        if (this->currentLevel <= 0.0f)
            this->currentLevel = 0.0f;

        return *this;
    }
    ADSR& ADSR::NoteOff() 
    { 
        this->currentState = State::Release;

        return *this;
    }

    float ADSR::GetCurrentVolumeModifier()
    {
        switch (this->currentState)
        {
            case State::Idle:
                break;

            case State::Attack:
                currentLevel += attackStep;
                if (currentLevel >= 1.0f)
                {
                    currentLevel = 1.0f;
                    currentState = State::Decay;
                }
                break;

            case State::Decay:
                this->currentLevel += this->decayStep * (this->sustainLevel - this->currentLevel);
                if (this->currentLevel <= this->sustainLevel + 0.001f)
                {
                    this->currentLevel = this->sustainLevel;
                    this->currentState = State::Sustain;
                }
                break;

            case State::Sustain:
                break;

            case State::Release:
                this->currentLevel += this->releaseStep * (0.0f - this->currentLevel);
                if (this->currentLevel <= 0.001f)
                {
                    this->currentLevel = 0.0f;
                    this->currentState = State::Idle;
                }
                break;
        }
        return this->currentLevel;
    }

    bool ADSR::IsActive() const 
    { 
        return this->currentState != State::Idle; 
    }

    bool ADSR::IsInRelease() const 
    { 
        return this->currentState == State::Release; 
    }

    void ADSR::RecalculateSteps()
    {
        attackStep = (attackTime > 0.f) ? (1.0f / (attackTime * sampleRate)) : 1.0f;
        decayStep = (decayTime > 0.f) ? 1.0f - expf(-1.0f / (decayTime * sampleRate)) : 1.0f;
        releaseStep = (releaseTime > 0.f) ? 1.0f - expf(-1.0f / (releaseTime * sampleRate)) : 1.0f;
    }

    //------------------------------------------------------------------------
} // namespace radiyx
