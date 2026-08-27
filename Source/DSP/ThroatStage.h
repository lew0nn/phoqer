#pragma once

#include "../Core/PhoqerTypes.h"
#include "../Voice/VoicePersonality.h"

namespace phoqer
{
class ThroatStage
{
public:
    void prepare(double newSampleRate) noexcept { sampleRate = static_cast<float>(newSampleRate); reset(); }
    void reset() noexcept { dcInput = dcOutput = low = band = 0.0f; }

    float process(float input, const MacroState& macros, const VocalState& vocal,
                  const VoicePersonality& personality) noexcept
    {
        constexpr float dcR = 0.995f;
        const auto dcBlocked = input - dcInput + dcR * dcOutput;
        dcInput = input;
        dcOutput = dcBlocked;

        const auto resonanceHz = 110.0f + 330.0f * (1.0f - macros.boom);
        const auto g = std::tan(juce::MathConstants<float>::pi
                              * juce::jmin(resonanceHz, sampleRate * 0.2f) / sampleRate);
        const auto k = 1.0f / (0.72f + 4.25f * macros.boom);
        const auto denominator = 1.0f + g * (g + k);
        const auto hp = (dcBlocked - (k + g) * band - low) / denominator;
        band += g * hp;
        low += g * band;
        band += g * hp;
        low += g * band;

        const auto body = dcBlocked + band * (0.12f + 0.68f * macros.boom);
        const auto drive = personality.throatDrive * personality.throatBias
                         * (0.92f + 2.15f * vocal.throatPressure
                            + 2.25f * vocal.barkTransient + 0.92f * macros.boom);
        const auto compressed = body / (1.0f + 0.7f * std::abs(body));
        const auto biased = compressed * drive + 0.06f * vocal.throatPressure;
        const auto saturated = std::tanh(biased) - std::tanh(0.06f * vocal.throatPressure);
        return juce::jlimit(-1.5f, 1.5f,
            saturated * (0.70f / std::sqrt(juce::jmax(1.0f, drive * 0.52f))));
    }

private:
    float sampleRate = 44100.0f;
    float dcInput = 0.0f, dcOutput = 0.0f, low = 0.0f, band = 0.0f;
};
}
