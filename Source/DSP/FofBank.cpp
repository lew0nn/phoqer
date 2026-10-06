#include "FofBank.h"

#include <algorithm>
#include <cmath>

namespace phoqer
{
void FofBank::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    for (auto& formant : formants)
        formant.prepare(sampleRate);
    pulse.prepare(sampleRate);
    highCoefficient = 1.0f - std::exp(-twoPi<float> * 2500.0f / static_cast<float>(sampleRate));
    reset();
}

void FofBank::reset() noexcept
{
    for (auto& formant : formants)
        formant.reset();
    pulse.reset();
    fundamentalPhase = 0.0;
    noiseLowPass = noiseBody = previousNoise = noiseHigh = 0.0f;
    lockedHarmonic.fill(0.0);
}

FormantSpec FofBank::shapeFormant(const FormantSpec& base, size_t index,
                                  const MacroState& macros, const VocalState& vocal,
                                  double fundamentalHz) noexcept
{
    FormantSpec spec = base;

    // Band 0 is the low source resonance; bands 1 and 2 are F1 and F2, which
    // carry almost all of the perceived vowel; bands 3 and 4 are air.
    //
    // The vowel is the call's own, moving one (VOWEL plus the mouth opening and
    // closing through the call), not the knob's fixed value: before, the motion
    // the behaviour engine drew never reached the sound. It walks a vowel path,
    // oo - oh - ah - eh - ee, as F1 and F2 ratios against each character's
    // measured formants, which sit at the default position. The ratios are the
    // human ones taken half way in log terms, so a seal stays a seal.
    if (index == 1 || index == 2)
    {
        static constexpr float path[5][2] = {
            { 0.64f, 0.89f },   // oo
            { 0.88f, 0.88f },   // oh
            { 1.00f, 1.00f },   // ah
            { 0.85f, 1.30f },   // eh
            { 0.61f, 1.45f } }; // ee
        static constexpr float home = 0.928f;   // the path at the default VOWEL, 0.35
        const auto position = clamp(0.0f, 1.0f, vocal.vowelMorph) * 4.0f;
        const auto lower = std::min(3, static_cast<int>(position));
        const auto fraction = position - static_cast<float>(lower);
        const auto column = index - 1;
        spec.frequency *= lerp(fraction, path[lower][column], path[lower + 1][column]) / home;
    }

    // BOOM is the size of the animal: a longer vocal tract puts every resonance
    // lower, so all five move together, about 20 percent either way from the
    // default (0.5). It also strengthens the source band: more chest.
    spec.frequency *= std::exp2((0.5f - macros.boom) * 0.56f);
    if (index == 0)
    {
        spec.gain *= 1.0f + 0.85f * macros.boom;
        spec.bandwidth *= lerp(macros.boom, 1.15f, 0.80f);
    }
    else if (index == 1)
    {
        spec.gain *= 1.0f + 0.40f * macros.boom;
    }

    // AIR brightens and softens the upper formants, gently: the reference
    // calls carry well under one percent of their energy above 4 kHz.
    if (index >= 3)
    {
        spec.gain *= 1.0f + 0.40f * macros.air;
        spec.bandwidth *= 1.0f + 0.50f * macros.air;
    }

    const auto settled = spec.frequency;   // before the attack's momentary push

    // The mouth opening and the bark transient push formants up and widen them,
    // which is what gives the attack its bite. The source band stays put.
    spec.frequency *= 1.0f + (index == 0 ? 0.0f : 0.06f * vocal.mouthOpen)
                    + (index >= 1 && index <= 2 ? 0.11f : 0.035f) * vocal.barkTransient;
    spec.bandwidth *= 1.0f + 0.50f * vocal.barkTransient;   // a gentler burst of brightness

    // Pull the centre onto a harmonic so a narrow formant keeps speaking as the
    // note moves. The harmonic is chosen once, at the start of the note, and
    // then followed: picking the nearest one every period made the band hop
    // from harmonic to harmonic as the vowel moved, so overtones lit up one
    // after another after the key. A narrow band never sits above the fourth
    // harmonic either: on a low note GROAN's singing formant landed on the
    // ninth or tenth and whistled over everything.
    if (spec.harmonicLock > 0.0f && fundamentalHz > 1.0)
    {
        auto& harmonic = lockedHarmonic[index];
        auto lock = clamp(0.0f, 1.0f, spec.harmonicLock);
        if (harmonic < 1.0)
        {
            const auto reference = noteFrequency > 1.0 ? noteFrequency : fundamentalHz;
            harmonic = std::max(1.0, std::floor(settled / reference + 0.5));
            if (base.bandwidth < 100.0f && harmonic > 4.0)
                harmonic = 4.0;
        }
        if (base.bandwidth < 100.0f && harmonic * fundamentalHz < 0.75 * settled)
            lock = 1.0f;    // pulled well below its home: sit right on the harmonic
        const auto locked = static_cast<float>(harmonic * fundamentalHz);
        spec.frequency = lerp(lock, spec.frequency, locked);
    }

    return spec;
}

float FofBank::process(double frequencyHz, const CharacterPreset& preset,
                       const MacroState& macros, const VocalState& vocal,
                       Random& random) noexcept
{
    // Retrigger every formant once per fundamental period. All the per-formant
    // shaping happens here, once per period, not once per sample.
    // BARK also sets the growl: the preset's jitter and period doubling scaled
    // from 0.4x to 2.1x (1x at the default 0.35), with extra doubling above 0.5
    // so even SQUEAL rasps when pushed.
    const auto growl = 0.4f + 1.7f * macros.bark;
    const auto rasp = 0.10f * std::max(0.0f, macros.bark - 0.5f);
    const auto tick = pulse.advance(frequencyHz, preset, random, growl, rasp);
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
    // Kept low: the reference calls have a weak fundamental, and a loud sine
    // here reads as a synth bass under the seal rather than as the seal.
    const auto fundamentalGain = preset.subOctaveGain + 0.06f * macros.boom;
    const auto fundamental = static_cast<float>(std::sin(twoPi<double> * fundamentalPhase))
                           * fundamentalGain;

    // Band-limited breath noise. The measured calls are almost entirely
    // voiced (spectral flatness 0.004 to 0.016): what sounds rough in them is
    // the pitch jitter, not hiss. So the floor is small and AIR brings breath
    // in from nothing, darker than before.
    const auto white = random.bipolar();
    const auto airCutoff = 1400.0f + 1600.0f * macros.air;    // breath, kept below about 3 kHz
    const auto bodyCutoff = 400.0f + 600.0f * macros.air;
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
        preset.noiseMix + 0.18f * macros.air * macros.air + 0.03f * vocal.barkTransient);

    // SQUEAL's squeak: the clip carries a noisy band above its few clean
    // harmonics for the length of each yelp, so it follows the call, not AIR.
    noiseHigh += highCoefficient * (white - noiseHigh);
    const auto squeak = (white - noiseHigh) * preset.squeakNoise
                      * (0.6f + 0.4f * vocal.barkTransient) * vocal.amplitudeShape;

    const auto value = (voiced * (1.0f - 0.20f * noiseGain)
                        + fundamental
                        + smoothNoise * noiseGain + squeak) * preset.outputTrim;

    return clamp(-4.0f, 4.0f, std::isfinite(value) ? value : 0.0f);
}
}
