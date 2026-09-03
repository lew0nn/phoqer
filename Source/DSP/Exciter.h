#pragma once

#include "../Core/PhoqerTypes.h"
#include "../Core/DspPrimitives.h"
#include "../Core/Random.h"
#include "../Voice/VoicePersonality.h"

namespace phoqer
{
class Exciter
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = static_cast<float>(newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        primaryPhase = 0.0f;
        secondaryPhase = 0.23f;
        noiseLowPass = noiseBody = previousNoise = 0.0f;
    }

    float process(float frequency, const MacroState& macros, const VocalState& vocal,
                  const VoicePersonality& personality, Random& random) noexcept
    {
        const auto safeFrequency = clamp(24.0f, sampleRate * 0.22f, frequency);
        advance(primaryPhase, safeFrequency / sampleRate);

        // DETUNE is a genuine optional second source. At zero its mix is exactly
        // zero, so the excitation is one phase-locked glottal frequency only.
        const auto detuneAmount = clamp(0.0f, 1.0f, macros.detune);
        const auto detuneCents = 4.0f + 22.0f * detuneAmount;
        const auto secondaryFrequency = safeFrequency
            * std::pow(2.0f, -detuneCents / 1200.0f);
        advance(secondaryPhase, secondaryFrequency / sampleRate);

        const auto pressure = clamp(0.0f, 1.0f, vocal.throatPressure);
        const auto lowRegister = clamp(0.0f, 1.0f,
            (420.0f - safeFrequency) / 300.0f);
        const auto brightness = clamp(0.0f, 1.0f,
            0.38f + 0.42f * macros.air + 0.45f * vocal.barkTransient
            + 0.18f * pressure + 0.32f * lowRegister);
        const auto primary = glottal(primaryPhase, safeFrequency, brightness);
        const auto secondary = glottal(secondaryPhase, secondaryFrequency, brightness);
        const auto detuneMix = 0.42f * std::pow(detuneAmount, 0.82f);
        const auto voiced = primary * (1.0f - 0.30f * detuneMix)
                          + secondary * detuneMix;

        const auto white = random.bipolar();
        const auto airCutoff = 2600.0f + 4200.0f * macros.air;
        const auto bodyCutoff = 540.0f + 900.0f * macros.air;
        const auto airCoefficient = 1.0f
            - std::exp(-twoPi<float> * airCutoff / sampleRate);
        const auto bodyCoefficient = 1.0f
            - std::exp(-twoPi<float> * bodyCutoff / sampleRate);
        noiseLowPass += airCoefficient * (white - noiseLowPass);
        noiseBody += bodyCoefficient * (white - noiseBody);
        const auto bandNoise = noiseLowPass - 0.82f * noiseBody;
        const auto smoothNoise = 0.70f * bandNoise + 0.30f * previousNoise;
        previousNoise = bandNoise;

        const auto airCharacter = std::sqrt(personality.airiness * personality.airBias);
        const auto breathGain = 0.015f
                              + 0.140f * macros.air * airCharacter
                              + 0.50f * vocal.barkTransient
                              + 0.080f * pressure;
        const auto voiceGain = 0.82f + 0.15f * macros.boom
                             + 0.15f * lowRegister;
        return voiced * voiceGain + smoothNoise * breathGain;
    }

private:
    float glottal(float phase, float frequency, float brightness) const noexcept
    {
        const auto angle = twoPi<float> * phase;
        // A seal call is not a pure sine. Keep every overtone phase-locked to
        // the played note, but give the upper harmonics enough energy to excite
        // several tract resonances at once.
        constexpr float fundamentalGain = 0.58f;
        float value = fundamentalGain * std::sin(angle);
        float energy = fundamentalGain * fundamentalGain;

        const auto addHarmonic = [&](int harmonic, float gain, float phaseOffset,
                                     float& sum, float& energySum)
        {
            if (frequency * static_cast<float>(harmonic) < sampleRate * 0.44f)
            {
                sum += gain * std::sin(angle * static_cast<float>(harmonic) + phaseOffset);
                energySum += gain * gain;
            }
        };

        addHarmonic(2, 0.62f + 0.20f * brightness, 0.14f, value, energy);
        addHarmonic(3, 0.44f + 0.24f * brightness, -0.08f, value, energy);
        addHarmonic(4, 0.28f + 0.22f * brightness, 0.19f, value, energy);
        addHarmonic(5, 0.16f + 0.18f * brightness, -0.16f, value, energy);
        addHarmonic(6, 0.09f + 0.14f * brightness, 0.07f, value, energy);
        addHarmonic(7, 0.05f + 0.11f * brightness, -0.11f, value, energy);
        addHarmonic(8, 0.08f * brightness, 0.05f, value, energy);
        addHarmonic(9, 0.06f * brightness, -0.07f, value, energy);
        addHarmonic(10, 0.05f * brightness, 0.09f, value, energy);
        addHarmonic(11, 0.035f * brightness, -0.05f, value, energy);
        addHarmonic(12, 0.025f * brightness, 0.03f, value, energy);
        return value * (0.82f / std::sqrt(std::max(0.20f, energy)));
    }

    static void advance(float& phase, float increment) noexcept
    {
        phase += increment;
        phase -= std::floor(phase);
    }

    float sampleRate = 44100.0f;
    float primaryPhase = 0.0f;
    float secondaryPhase = 0.23f;
    float noiseLowPass = 0.0f;
    float noiseBody = 0.0f;
    float previousNoise = 0.0f;
};
}
