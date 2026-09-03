#pragma once

#include <algorithm>
#include <cmath>

namespace phoqer
{
template <typename Type>
constexpr Type clamp(Type minimum, Type maximum, Type value) noexcept
{
    return std::clamp(value, minimum, maximum);
}

template <typename Type>
constexpr Type lerp(Type amount, Type start, Type end) noexcept
{
    return start + amount * (end - start);
}

template <typename Type>
constexpr Type pi = static_cast<Type>(3.14159265358979323846264338327950288L);

template <typename Type>
constexpr Type twoPi = static_cast<Type>(2) * pi<Type>;

inline float midiNoteToHz(float note) noexcept
{
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

inline float decibelsToGain(float decibels) noexcept
{
    return std::pow(10.0f, decibels / 20.0f);
}

class LinearSmoother
{
public:
    void reset(double newSampleRate, double rampSeconds) noexcept
    {
        rampLength = std::max(1, static_cast<int>(std::round(newSampleRate * rampSeconds)));
        remaining = 0;
        step = 0.0f;
    }

    void setCurrentAndTargetValue(float value) noexcept
    {
        current = target = value;
        remaining = 0;
        step = 0.0f;
    }

    void setTargetValue(float value) noexcept
    {
        target = value;
        remaining = rampLength;
        step = (target - current) / static_cast<float>(remaining);
    }

    float getNextValue() noexcept
    {
        if (remaining > 0)
        {
            current += step;
            if (--remaining == 0)
                current = target;
        }
        return current;
    }

private:
    float current = 0.0f;
    float target = 0.0f;
    float step = 0.0f;
    int rampLength = 1;
    int remaining = 0;
};

class AdsrEnvelope
{
public:
    struct Parameters
    {
        float attack = 0.01f;
        float decay = 0.1f;
        float sustain = 1.0f;
        float release = 0.1f;
    };

    void setSampleRate(double value) noexcept { sampleRate = std::max(1.0, value); }
    void setParameters(const Parameters& value) noexcept { parameters = value; }

    void noteOn() noexcept
    {
        state = parameters.attack > 0.0f ? State::attack : State::decay;
        if (state == State::decay)
            level = 1.0f;
        updateStep();
    }

    void noteOff() noexcept
    {
        if (state == State::idle)
            return;
        state = State::release;
        updateStep();
    }

    void reset() noexcept
    {
        state = State::idle;
        level = 0.0f;
        step = 0.0f;
    }

    bool isActive() const noexcept { return state != State::idle; }

    float getNextSample() noexcept
    {
        switch (state)
        {
            case State::idle: return 0.0f;
            case State::attack:
                level += step;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    state = State::decay;
                    updateStep();
                }
                break;
            case State::decay:
                level += step;
                if (level <= parameters.sustain)
                {
                    level = parameters.sustain;
                    state = State::sustain;
                }
                break;
            case State::sustain:
                level = parameters.sustain;
                break;
            case State::release:
                level += step;
                if (level <= 0.0f)
                    reset();
                break;
        }
        return level;
    }

private:
    enum class State { idle, attack, decay, sustain, release };

    void updateStep() noexcept
    {
        if (state == State::attack)
            step = 1.0f / std::max(1.0f, parameters.attack * static_cast<float>(sampleRate));
        else if (state == State::decay)
            step = (parameters.sustain - level)
                 / std::max(1.0f, parameters.decay * static_cast<float>(sampleRate));
        else if (state == State::release)
            step = -level / std::max(1.0f, parameters.release * static_cast<float>(sampleRate));
    }

    Parameters parameters;
    State state = State::idle;
    double sampleRate = 44100.0;
    float level = 0.0f;
    float step = 0.0f;
};
}
