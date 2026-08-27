#include "FormantBank.h"

namespace phoqer
{
namespace
{
constexpr std::array<FormantBank::VowelDescription, 5> vowelAnchors {{
    { { 365.0f,  805.0f, 2140.0f, 3150.0f }, { 5.4f, 7.8f, 8.8f, 9.6f }, { 0.92f, 0.55f, 0.25f, 0.15f } }, // U
    { { 525.0f,  980.0f, 2360.0f, 3400.0f }, { 5.0f, 7.2f, 8.4f, 9.2f }, { 0.84f, 0.62f, 0.29f, 0.17f } }, // O
    { { 805.0f, 1340.0f, 2600.0f, 3660.0f }, { 4.3f, 6.2f, 7.6f, 8.6f }, { 0.78f, 0.68f, 0.36f, 0.21f } }, // A
    { { 610.0f, 1790.0f, 2780.0f, 3890.0f }, { 5.4f, 8.3f, 9.0f, 9.6f }, { 0.66f, 0.79f, 0.40f, 0.23f } }, // E
    { { 385.0f, 2210.0f, 3070.0f, 4260.0f }, { 6.3f, 9.6f, 10.2f, 10.8f }, { 0.53f, 0.86f, 0.46f, 0.27f } }  // I
}};

FormantBank::VowelDescription interpolateAnchor(float morph) noexcept
{
    const auto position = juce::jlimit(0.0f, 1.0f, morph) * 4.0f;
    const auto lower = juce::jlimit(0, 4, static_cast<int>(position));
    const auto upper = juce::jmin(4, lower + 1);
    const auto fraction = position - static_cast<float>(lower);
    FormantBank::VowelDescription result {};
    for (size_t formant = 0; formant < 4; ++formant)
    {
        result.frequency[formant] = juce::jmap(fraction,
            vowelAnchors[static_cast<size_t>(lower)].frequency[formant],
            vowelAnchors[static_cast<size_t>(upper)].frequency[formant]);
        result.q[formant] = juce::jmap(fraction,
            vowelAnchors[static_cast<size_t>(lower)].q[formant],
            vowelAnchors[static_cast<size_t>(upper)].q[formant]);
        result.gain[formant] = juce::jmap(fraction,
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
    const auto safeFrequency = juce::jlimit(40.0f, sampleRate * 0.43f, frequency);
    const auto g = std::tan(juce::MathConstants<float>::pi * safeFrequency / sampleRate);
    const auto k = 1.0f / juce::jmax(0.5f, q);
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
                           const VocalState& vocal, const VoicePersonality& personality,
                           float tideMovement) noexcept
{
    const auto sampleRate = static_cast<float>(currentSampleRate);
    const auto anchor = interpolateAnchor(vocal.vowelMorph);
    const auto registerScale = juce::jlimit(0.90f, 1.12f, 1.0f + 0.0020f * (midiNote - 60.0f));
    const auto bodyScale = 1.16f - 0.30f * macros.boom;
    const auto wander = 1.0f + tideMovement * macros.tide * 0.018f;
    const auto frequencySmoothing = 1.0f - std::exp(-1.0f / (0.014f * sampleRate));
    const auto shapeSmoothing = 1.0f - std::exp(-1.0f / (0.024f * sampleRate));
    const std::array<float, 4> closedScale { 0.78f, 0.93f, 0.98f, 1.00f };
    const std::array<float, 4> openScale { 1.22f, 1.09f, 1.05f, 1.035f };
    const std::array<float, 4> boomLowScale { 0.94f, 0.96f, 1.0f, 1.0f };

    float result = input * (0.035f + 0.018f * (1.0f - macros.air));
    for (size_t i = 0; i < resonators.size(); ++i)
    {
        const auto mouthScale = juce::jmap(vocal.mouthOpen, closedScale[i], openScale[i]);
        const auto barkShift = 1.0f + vocal.barkTransient * (i < 2 ? 0.11f : 0.035f);
        auto targetFrequency = (anchor.frequency[i] * personality.formantScale
                              + personality.formantOffset)
                             * registerScale * bodyScale
                             * juce::jmap(macros.boom, 1.0f, boomLowScale[i])
                             * mouthScale * barkShift * wander;

        auto targetQ = anchor.q[i]
                     * (1.0f - 0.22f * vocal.mouthOpen - 0.30f * vocal.barkTransient
                        + 0.08f * juce::jlimit(0.0f, 1.0f,
                            (vocal.callPhase - 0.58f) / 0.28f));
        if (i >= 2)
            targetQ *= 1.0f - 0.13f * macros.air;
        targetQ = juce::jlimit(2.2f, 13.0f, targetQ);

        auto targetGain = anchor.gain[i];
        if (i == 0) targetGain *= 0.82f + 0.42f * macros.boom;
        if (i == 1) targetGain *= 0.92f + 0.22f * vocal.mouthOpen;
        if (i >= 2) targetGain *= 0.72f + 0.72f * macros.air;
        if (i < 2) targetGain *= 1.0f + 0.24f * vocal.barkTransient;
        targetGain *= 1.0f - 0.08f * macros.boom;

        auto& resonator = resonators[i];
        resonator.smoothedFrequency += (targetFrequency - resonator.smoothedFrequency) * frequencySmoothing;
        resonator.smoothedQ += (targetQ - resonator.smoothedQ) * shapeSmoothing;
        resonator.smoothedGain += (targetGain - resonator.smoothedGain) * shapeSmoothing;
        result += resonator.smoothedGain
                * resonator.process(input, resonator.smoothedFrequency, resonator.smoothedQ, sampleRate);
    }
    return juce::jlimit(-2.0f, 2.0f, result * 0.64f);
}
}
