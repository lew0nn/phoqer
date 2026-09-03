#pragma once

#include "../Core/PhoqerTypes.h"
#include "../Core/DspPrimitives.h"
#include "../Voice/VoicePersonality.h"

namespace phoqer
{
class ThroatStage
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = static_cast<float>(newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        dcInput = dcOutput = low = band = 0.0f;
    }

    float process(float input, const MacroState& macros, const VocalState& vocal,
                  const VoicePersonality&) noexcept
    {
        constexpr float dcR = 0.995f;
        const auto dcBlocked = input - dcInput + dcR * dcOutput;
        dcInput = input;
        dcOutput = dcBlocked;

        const auto resonanceHz = 440.0f + 170.0f * (1.0f - macros.boom);
        const auto g = std::tan(pi<float>
                              * std::min(resonanceHz, sampleRate * 0.2f) / sampleRate);
        constexpr float k = 0.72f;
        const auto denominator = 1.0f + g * (g + k);
        const auto high = (dcBlocked - (k + g) * band - low) / denominator;
        band += g * high;
        low += g * band;
        band += g * high;
        low += g * band;

        const auto bodyMix = 0.08f + 0.22f * macros.boom;
        const auto pressureGain = 0.92f + 0.12f * vocal.throatPressure;
        return clamp(-2.0f, 2.0f,
            (dcBlocked * (1.0f - 0.18f * bodyMix) + band * bodyMix) * pressureGain);
    }

private:
    float sampleRate = 44100.0f;
    float dcInput = 0.0f;
    float dcOutput = 0.0f;
    float low = 0.0f;
    float band = 0.0f;
};
}
