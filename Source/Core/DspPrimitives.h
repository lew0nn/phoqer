#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace phoqer
{
// The per-sample helpers below are small and called from the hottest loops; MSVC would otherwise
// leave them as calls.
#if defined(_MSC_VER)
 #define PHOQER_HOT_INLINE __forceinline
#else
 #define PHOQER_HOT_INLINE inline __attribute__((always_inline))
#endif

// x - floor(x), without the library call (exact for |x| below 2^63).
PHOQER_HOT_INLINE double wrapPhase(double x) noexcept
{
    auto whole = static_cast<double>(static_cast<long long>(x));
    if (whole > x) whole -= 1.0;
    return x - whole;
}

// std::isfinite without the library call: false for infinities and NaN.
PHOQER_HOT_INLINE bool isFiniteSample(float x) noexcept
{
    uint32_t bits;
    std::memcpy(&bits, &x, sizeof bits);
    return (bits & 0x7f800000u) != 0x7f800000u;
}

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

// Fast 2^x, e^x and sin for the per-sample paths. They agree with the library functions to
// about one part in ten million, below what a 24-bit file can hold, at a fraction of the cost.
PHOQER_HOT_INLINE float fastExp2(float x) noexcept
{
    if (x < -126.0f) return 0.0f;
    x = std::min(x, 127.0f);
    const auto whole = static_cast<int>(x + (x >= 0.0f ? 0.5f : -0.5f));    // nearest integer
    const auto y = (x - static_cast<float>(whole)) * 0.693147181f;           // the rest, as a power of e: |y| <= 0.35
    const auto fraction = 1.0f + y * (1.0f + y * (0.5f + y * (1.0f / 6.0f + y * (1.0f / 24.0f
                        + y * (1.0f / 120.0f + y * (1.0f / 720.0f + y * (1.0f / 5040.0f)))))));
    const auto bits = static_cast<uint32_t>(whole + 127) << 23;              // 2^whole, built directly
    float scale;
    std::memcpy(&scale, &bits, sizeof scale);
    return fraction * scale;
}

PHOQER_HOT_INLINE float fastExp(float x) noexcept { return fastExp2(x * 1.44269504f); }

// sin(2 pi cycles), for a phase counted in cycles.
PHOQER_HOT_INLINE float fastSin2Pi(double cycles) noexcept
{
    auto whole = static_cast<double>(static_cast<long long>(cycles));
    if (whole > cycles) whole -= 1.0;
    auto q = static_cast<float>(cycles - whole);                             // [0, 1)
    if (q > 0.5f) q -= 1.0f;                                                 // [-0.5, 0.5]
    if (q > 0.25f) q = 0.5f - q;                                             // sin(pi - a) = sin(a)
    else if (q < -0.25f) q = -0.5f - q;
    const auto a = q * twoPi<float>, a2 = a * a;                             // |a| <= pi / 2
    return a * (1.0f + a2 * (-1.0f / 6.0f + a2 * (1.0f / 120.0f + a2 * (-1.0f / 5040.0f
              + a2 * (1.0f / 362880.0f + a2 * (-1.0f / 39916800.0f))))));
}

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
        if (value == target) return;
        target = value;
        remaining = rampLength;
        step = (target - current) / static_cast<float>(remaining);
    }

    float getCurrentValue() const noexcept { return current; }

    // Steps count samples (at least 1) and returns the value after them.
    float skip(int count) noexcept
    {
        if (remaining <= 0)
            return current;
        for (int i = 1; i < count; ++i)
            getNextValue();
        return getNextValue();
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

    // Release from wherever it is, over this many seconds (a stolen voice's quick fade).
    void releaseIn(float seconds) noexcept
    {
        if (state == State::idle)
            return;
        parameters.release = seconds;
        state = State::release;
        updateStep();
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
