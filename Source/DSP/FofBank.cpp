#include "FofBank.h"

#include <cmath>

namespace phoqer
{
void FofBank::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    for (auto& formant : formants)
        formant.prepare(sampleRate);
    pulse.prepare(sampleRate);
    reset();
}

void FofBank::reset() noexcept
{
    for (auto& formant : formants)
        formant.reset();
    pulse.reset();
    fundamentalPhase = 0.0;
    noiseLowPass = noiseBody = previousNoise = 0.0f;
}

FormantSpec FofBank::shapeFormant(const FormantSpec& base, size_t index,
                                  const MacroState& macros, const VocalState& vocal,
                                  double fundamentalHz) noexcept
{
    FormantSpec spec = base;

    // Band 0 is the low source resonance; bands 1 and 2 are F1 and F2, which
    // carry almost all of the perceived vowel; bands 3 and 4 are air.
    const auto vowel = clamp(0.0f, 1.0f, macros.vowel);
    if (index == 1)
        spec.frequency *= lerp(vowel, 0.86f, 1.18f);
    else if (index == 2)
        spec.frequency *= lerp(vowel, 0.90f, 1.30f);

    // BOOM strengthens the source and pulls the first formant down: chest
    // rather than throat.
    if (index == 0)
    {
        spec.gain *= 1.0f + 0.85f * macros.boom;
        spec.bandwidth *= lerp(macros.boom, 1.15f, 0.80f);
    }
    else if (index == 1)
    {
        spec.frequency *= lerp(macros.boom, 1.06f, 0.92f);
        spec.gain *= 1.0f + 0.40f * macros.boom;
    }

    // AIR brightens and softens the upper formants.
    if (index >= 3)
    {
        spec.gain *= 1.0f + 1.20f * macros.air;
        spec.bandwidth *= 1.0f + 0.50f * macros.air;
    }

    // The mouth opening and the bark transient push formants up and widen them,
    // which is what gives the attack its bite. The source band stays put.
    spec.frequency *= 1.0f + (index == 0 ? 0.0f : 0.06f * vocal.mouthOpen)
                    + (index >= 1 && index <= 2 ? 0.11f : 0.035f) * vocal.barkTransient;
    spec.bandwidth *= 1.0f + 0.90f * vocal.barkTransient;

    // Pull the centre onto the nearest harmonic so a narrow formant keeps
    // speaking as the note moves. Done last, so it acts on the shaped centre.
    if (spec.harmonicLock > 0.0f && fundamentalHz > 1.0)
    {
        auto harmonic = std::floor(spec.frequency / fundamentalHz + 0.5);
        if (harmonic < 1.0)
            harmonic = 1.0;
        const auto locked = static_cast<float>(harmonic * fundamentalHz);
        spec.frequency = lerp(clamp(0.0f, 1.0f, spec.harmonicLock), spec.frequency, locked);
    }

    return spec;
}

float FofBank::process(double frequencyHz, const CharacterPreset& preset,
                       const MacroState& macros, const VocalState& vocal,
                       Random& random) noexcept
{
    // Retrigger every formant once per fundamental period. All the per-formant
    // shaping happens here, once per period, not once per sample.
    const auto tick = pulse.advance(frequencyHz, preset, random);
    if (tick.trigger)
    {
        const auto skirt = static_cast<double>(preset.attackSkirtSeconds)
                         * (1.0 - 0.35 * clamp(0.0f, 1.0f, macros.air));
        for (size_t index = 0; index < formants.size(); ++index)
            formants[index].trigger(
                shapeFormant(preset.formants[index], index, macros, vocal, frequencyHz),
                tick.periodSamples, skirt, tick.amplitude);
    }

    float voiced = 0.0f;
    for (auto& formant : formants)
        voiced += formant.process();

    // The fundamental sine. FOF grains put energy only near the formants, so
    // without this a bass character with a 1 kHz first formant has no low end.
    const auto safeFrequency = clamp(20.0, sampleRate * 0.45, frequencyHz);
    fundamentalPhase += safeFrequency / sampleRate;
    fundamentalPhase -= std::floor(fundamentalPhase);
    const auto fundamentalGain = preset.subOctaveGain + 0.16f * macros.boom;
    const auto fundamental = static_cast<float>(std::sin(twoPi<double> * fundamentalPhase))
                           * fundamentalGain;

    // Band-limited breath noise.
    const auto white = random.bipolar();
    const auto airCutoff = 2600.0f + 4200.0f * macros.air;
    const auto bodyCutoff = 540.0f + 900.0f * macros.air;
    const auto airCoefficient = 1.0f
        - std::exp(-twoPi<float> * airCutoff / static_cast<float>(sampleRate));
    const auto bodyCoefficient = 1.0f
        - std::exp(-twoPi<float> * bodyCutoff / static_cast<float>(sampleRate));
    noiseLowPass += airCoefficient * (white - noiseLowPass);
    noiseBody += bodyCoefficient * (white - noiseBody);
    const auto bandNoise = noiseLowPass - 0.82f * noiseBody;
    const auto smoothNoise = 0.70f * bandNoise + 0.30f * previousNoise;
    previousNoise = bandNoise;

    const auto noiseGain = clamp(0.0f, 1.0f,
        preset.noiseMix * (1.0f + 0.9f * macros.air) + 0.45f * vocal.barkTransient);

    const auto value = (voiced * (1.0f - 0.35f * noiseGain)
                        + fundamental
                        + smoothNoise * noiseGain * 1.6f) * preset.outputTrim;

    return clamp(-4.0f, 4.0f, std::isfinite(value) ? value : 0.0f);
}
}
