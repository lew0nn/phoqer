#pragma once

#include "../Core/PhoqerTypes.h"
#include "../Core/Random.h"

namespace phoqer
{
class Exciter
{
public:
    void prepare(double newSampleRate) noexcept { sampleRate = static_cast<float>(newSampleRate); reset(); }
    void reset() noexcept { phase = subPhase = noiseLowPass = noiseSlow = previousNoise = 0.0f; }

    float process(float frequency, const MacroState& macros, const VocalState& vocal,
                  const VoicePersonality& personality, Random& random) noexcept
    {
        const auto increment = juce::jlimit(0.0f, 0.45f, frequency / sampleRate);
        phase += increment;
        if (phase >= 1.0f) phase -= 1.0f;

        const auto pulseWidth = juce::jlimit(0.22f, 0.52f,
            0.30f + 0.15f * macros.boom - 0.07f * vocal.throatPressure);
        auto saw = 2.0f * phase - 1.0f;
        saw -= polyBlep(phase, increment);
        auto pulse = phase < pulseWidth ? 1.0f : -1.0f;
        pulse += polyBlep(phase, increment);
        auto fallingPhase = phase - pulseWidth;
        if (fallingPhase < 0.0f) fallingPhase += 1.0f;
        pulse -= polyBlep(fallingPhase, increment);

        subPhase += 0.5f * increment;
        if (subPhase >= 1.0f) subPhase -= 1.0f;
        const auto sub = std::sin(juce::MathConstants<float>::twoPi * subPhase);

        const auto white = random.bipolar();
        const auto fastCutoff = 2100.0f + 7900.0f * macros.air;
        const auto slowCutoff = 480.0f + 1550.0f * macros.air;
        const auto fastCoefficient = 1.0f - std::exp(
            -juce::MathConstants<float>::twoPi * fastCutoff / sampleRate);
        const auto slowCoefficient = 1.0f - std::exp(
            -juce::MathConstants<float>::twoPi * slowCutoff / sampleRate);
        noiseLowPass += fastCoefficient * (white - noiseLowPass);
        noiseSlow += slowCoefficient * (white - noiseSlow);
        const auto texturedNoise = 0.76f * (noiseLowPass - 0.72f * noiseSlow) + 0.24f * noiseSlow;
        const auto softenedNoise = 0.72f * texturedNoise + 0.28f * previousNoise;
        previousNoise = noiseLowPass;

        const auto voiced = pulse * juce::jmap(macros.boom, 0.45f, 0.68f)
                          + saw * juce::jmap(macros.boom, 0.48f, 0.27f)
                          + sub * (0.012f + 0.17f * std::pow(macros.boom, 1.35f));
        const auto airCharacter = std::sqrt(personality.airiness * personality.airBias);
        const auto breathAmount = 0.004f
                                + 0.40f * std::pow(macros.air, 1.08f) * airCharacter
                                + 0.43f * vocal.barkTransient;
        const auto breath = softenedNoise * breathAmount;
        const auto voicedGain = 0.61f - 0.27f * macros.air + 0.06f * macros.boom;
        return voiced * voicedGain + breath;
    }

private:
    static float polyBlep(float t, float dt) noexcept
    {
        if (dt <= 0.0f) return 0.0f;
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float sampleRate = 44100.0f, phase = 0.0f, subPhase = 0.0f;
    float noiseLowPass = 0.0f, noiseSlow = 0.0f, previousNoise = 0.0f;
};
}
