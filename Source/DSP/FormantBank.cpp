#include "FormantBank.h"
#include "../Core/DspPrimitives.h"

#include <algorithm>
#include <cmath>

namespace phoqer
{
namespace
{
constexpr std::array<FormantBank::VowelDescription, 5> vowelAnchors {{
    { { 500.0f,  900.0f, 1450.0f, 2200.0f }, { 5.0f, 5.8f, 6.3f, 5.5f }, { 0.42f, 0.50f, 0.32f, 0.11f } }, // U
    { { 550.0f,  980.0f, 1570.0f, 2350.0f }, { 4.9f, 6.0f, 6.6f, 5.7f }, { 0.44f, 0.58f, 0.36f, 0.13f } }, // O
    { { 620.0f, 1100.0f, 1700.0f, 2500.0f }, { 4.7f, 5.8f, 6.2f, 5.5f }, { 0.40f, 0.62f, 0.40f, 0.14f } }, // A
    { { 560.0f, 1230.0f, 1820.0f, 2700.0f }, { 5.1f, 6.2f, 6.8f, 5.9f }, { 0.36f, 0.63f, 0.38f, 0.14f } }, // E
    { { 520.0f, 1400.0f, 1950.0f, 2950.0f }, { 5.3f, 6.5f, 7.0f, 6.1f }, { 0.32f, 0.64f, 0.34f, 0.15f } }  // I
}};

FormantBank::VowelDescription interpolateAnchor(float morph) noexcept
{
    const auto position = clamp(0.0f, 1.0f, morph) * 4.0f;
    const auto lower = clamp(0, 4, static_cast<int>(position));
    const auto upper = std::min(4, lower + 1);
    const auto fraction = position - static_cast<float>(lower);
    FormantBank::VowelDescription result {};
    for (size_t formant = 0; formant < 4; ++formant)
    {
        result.frequency[formant] = lerp(fraction,
            vowelAnchors[static_cast<size_t>(lower)].frequency[formant],
            vowelAnchors[static_cast<size_t>(upper)].frequency[formant]);
        result.q[formant] = lerp(fraction,
            vowelAnchors[static_cast<size_t>(lower)].q[formant],
            vowelAnchors[static_cast<size_t>(upper)].q[formant]);
        result.gain[formant] = lerp(fraction,
            vowelAnchors[static_cast<size_t>(lower)].gain[formant],
            vowelAnchors[static_cast<size_t>(upper)].gain[formant]);
    }
    return result;
}
}

FormantBank::VowelDescription FormantBank::describeVowel(float morph) noexcept
{
    return interpolateAnchor(morph);
}

void FormantBank::prepare(double sampleRate)
{
    currentSampleRate = sampleRate;
    reset();
}

void FormantBank::reset() noexcept
{
    for (size_t i = 0; i < resonators.size(); ++i)
    {
        resonators[i].reset();
        resonators[i].smoothedFrequency = vowelAnchors[0].frequency[i];
        resonators[i].smoothedQ = vowelAnchors[0].q[i];
        resonators[i].smoothedGain = vowelAnchors[0].gain[i];
    }
}

float FormantBank::Resonator::process(float input, float frequency, float q, float sampleRate) noexcept
{
    const auto safeFrequency = clamp(40.0f, sampleRate * 0.43f, frequency);
    const auto g = std::tan(pi<float> * safeFrequency / sampleRate);
    const auto k = 1.0f / std::max(0.5f, q);
    const auto a1 = 1.0f / (1.0f + g * (g + k));
    const auto v1 = a1 * (ic1eq + g * (input - ic2eq));
    const auto v2 = ic2eq + g * v1;
    ic1eq = 2.0f * v1 - ic1eq;
    ic2eq = 2.0f * v2 - ic2eq;
    if (! std::isfinite(ic1eq) || ! std::isfinite(ic2eq))
        reset();
    return v1;
}

float FormantBank::process(float input, float midiNote, const MacroState& macros,
                           const VocalState& vocal, const VoicePersonality&,
                           float) noexcept
{
    const auto sampleRate = static_cast<float>(currentSampleRate);
    const auto anchor = interpolateAnchor(vocal.vowelMorph);
    const auto lowRegister = clamp(0.0f, 1.0f, (67.0f - midiNote) / 24.0f);
    const auto registerSemitones = midiNote < 67.0f
        ? (midiNote - 67.0f) * 0.20f
        : (midiNote - 67.0f) * 0.04f;
    const auto registerScale = clamp(0.76f, 1.06f,
        std::pow(2.0f, registerSemitones / 12.0f));
    const auto bodyScale = 1.02f - 0.04f * macros.boom;
    const auto frequencySmoothing = 1.0f - std::exp(-1.0f / (0.014f * sampleRate));
    const auto shapeSmoothing = 1.0f - std::exp(-1.0f / (0.024f * sampleRate));
    const std::array<float, 4> closedScale { 0.96f, 0.98f, 0.99f, 1.00f };
    const std::array<float, 4> openScale { 1.04f, 1.025f, 1.018f, 1.01f };
    const std::array<float, 4> boomLowScale { 0.98f, 0.99f, 1.0f, 1.0f };

    // Keep only enough direct glottal signal for pitch definition; the resonant
    // vocal tract now supplies the identity instead of additive modal oscillators.
    float result = input * (0.30f + 0.07f * macros.boom
                          + 0.16f * lowRegister);
    for (size_t i = 0; i < resonators.size(); ++i)
    {
        const auto mouthScale = lerp(vocal.mouthOpen, closedScale[i], openScale[i]);
        const auto barkShift = 1.0f + vocal.barkTransient * (i < 2 ? 0.11f : 0.035f);
        auto targetFrequency = anchor.frequency[i] * registerScale * bodyScale
                             * lerp(macros.boom, 1.0f, boomLowScale[i])
                             * mouthScale * barkShift;

        auto targetQ = anchor.q[i]
                     * (1.0f - 0.10f * vocal.mouthOpen - 0.16f * vocal.barkTransient
                        + 0.04f * clamp(0.0f, 1.0f,
                            (vocal.callPhase - 0.58f) / 0.28f));
        targetQ *= 1.0f - 0.28f * lowRegister;
        if (i >= 2)
            targetQ *= 1.0f - 0.08f * macros.air;
        targetQ = clamp(3.0f, 8.0f, targetQ);

        auto targetGain = anchor.gain[i];
        if (i == 0) targetGain *= 0.90f + 0.22f * macros.boom;
        if (i == 1) targetGain *= 0.96f + 0.12f * vocal.mouthOpen;
        if (i == 2) targetGain *= 1.05f + 0.40f * macros.air;
        if (i == 3) targetGain *= 0.82f + 0.55f * macros.air;
        if (i < 3) targetGain *= 1.0f + 0.28f * vocal.barkTransient;
        if (i == 0) targetGain *= 1.0f + 0.65f * lowRegister;
        if (i == 1) targetGain *= 1.0f + 0.52f * lowRegister;
        if (i == 2) targetGain *= 1.0f + 0.20f * lowRegister;

        auto& resonator = resonators[i];
        resonator.smoothedFrequency += (targetFrequency - resonator.smoothedFrequency) * frequencySmoothing;
        resonator.smoothedQ += (targetQ - resonator.smoothedQ) * shapeSmoothing;
        resonator.smoothedGain += (targetGain - resonator.smoothedGain) * shapeSmoothing;
        result += resonator.smoothedGain
                * resonator.process(input, resonator.smoothedFrequency, resonator.smoothedQ, sampleRate);
    }
    return clamp(-2.0f, 2.0f, result * 0.92f);
}
}
