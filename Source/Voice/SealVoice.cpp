#include "SealVoice.h"

#include <cmath>

namespace phoqer
{
namespace
{
// What each behaviour does to a held note over time: a gain, a pitch offset in
// semitones, a push on the vowel, and an extra bark kick. CALL is one call;
// the others keep moving for as long as the key is held.
struct ModeMotion
{
    float gain = 1.0f, semitones = 0.0f, vowel = 0.0f, kick = 0.0f;
    float muffle = 0.0f;    // 0 open, 1 a closed-mouth hum (MURMUR)
};

ModeMotion modeMotion(BehaviourMode mode, float t) noexcept
{
    ModeMotion m;
    const auto cycle = [t](float rateHz) { const auto p = t * rateHz; return (p - std::floor(p)) / rateHz; };
    switch (mode)
    {
        case BehaviourMode::honk:      // separate honks, four a second: a nasal hit that drops away to near silence
        {
            const auto c = cycle(4.0f);
            const auto hit = std::exp(-c / 0.07f) * std::min(1.0f, c / 0.006f);
            m.gain = 0.05f + 0.95f * hit;
            m.semitones = 2.5f * std::exp(-c / 0.04f);
            m.vowel = -0.25f + 0.20f * std::exp(-c / 0.06f);
            m.kick = 0.30f * std::exp(-c / 0.025f);
            m.muffle = 0.45f;                                 // half-closed: a nasal honk, not a bark
            break;
        }
        case BehaviourMode::bark:      // separate hard barks, two and a half a second, silence between
        {
            const auto c = cycle(2.5f);
            // a fast rise, a full 60 ms, then the fall: a bark, not a punch
            const auto open = std::min(1.0f, c / 0.006f) * (c < 0.06f ? 1.0f : std::exp(-(c - 0.06f) / 0.08f));
            const auto shut = 1.0f - std::min(1.0f, std::max(0.0f, (c - 0.16f) / 0.04f));
            m.gain = 0.03f + 0.97f * open * shut;
            m.semitones = 2.5f * std::exp(-c / 0.04f);
            m.kick = 0.25f * std::exp(-c / 0.025f);
            m.vowel = 0.15f;                                  // wide open: a bark, not a honk
            break;
        }
        case BehaviourMode::wail:      // a long glide: up two and a half semitones and back, every 3.2 s
        {
            const auto s = std::sin(twoPi<float> * t / 3.2f);
            m.semitones = 2.4f * s;
            m.vowel = 0.35f * std::max(0.0f, s);              // the mouth opens as it climbs
            m.gain = 0.90f + 0.25f * std::max(0.0f, s);       // and it sings out at the top
            break;
        }
        case BehaviourMode::murmur:    // a closed-mouth "mmm"; the voice adds the muttered syllables
            m.vowel = -0.35f;
            m.muffle = 1.0f;
            break;
        case BehaviourMode::call:
            break;
    }
    return m;
}
}

SealVoice::SealVoice(uint32_t seed, uint64_t& globalAgeCounter) noexcept
    : random(seed), ageCounter(globalAgeCounter)
{
    // A fixed per-voice pan and detune direction. Eight voices then decorrelate
    // on their own, which is what makes the dry path stereo without a widener.
    const auto panPosition = random.range(-0.6f, 0.6f);
    panLeft = std::sqrt(0.5f * (1.0f - panPosition));
    panRight = std::sqrt(0.5f * (1.0f + panPosition));
    detuneOffsetSemitones = random.bipolar();
}

void SealVoice::prepare(double newSampleRate, int)
{
    sampleRate = newSampleRate;
    amplitudeEnvelope.setSampleRate(sampleRate);
    fofBank.prepare(sampleRate);
    // A gentle 7 kHz roll-off on each voice: the reference calls carry almost
    // nothing up there, and what the grains put there reads as digital fizz.
    polishCoefficient = 1.0f - std::exp(-twoPi<float> * 7000.0f / static_cast<float>(sampleRate));
    muffleCoefficient = 1.0f - std::exp(-twoPi<float> * 900.0f / static_cast<float>(sampleRate));
    for (auto* smoother : { &smoothBoom, &smoothAir, &smoothBark, &smoothVowel,
                            &smoothTide, &smoothDetune })
        smoother->reset(sampleRate, 0.035);
    smoothBoom.setCurrentAndTargetValue(macros.boom);
    smoothAir.setCurrentAndTargetValue(macros.air);
    smoothBark.setCurrentAndTargetValue(macros.bark);
    smoothVowel.setCurrentAndTargetValue(macros.vowel);
    smoothTide.setCurrentAndTargetValue(macros.tide);
    smoothDetune.setCurrentAndTargetValue(macros.detune);
    hardReset();
}

void SealVoice::setMacros(const MacroState& newMacros) noexcept
{
    macros = newMacros;
    smoothBoom.setTargetValue(macros.boom);
    smoothAir.setTargetValue(macros.air);
    smoothBark.setTargetValue(macros.bark);
    smoothVowel.setTargetValue(macros.vowel);
    smoothTide.setTargetValue(macros.tide);
    smoothDetune.setTargetValue(macros.detune);
}

void SealVoice::resetDsp() noexcept
{
    fofBank.reset();
    previousOutput = 0.0f;
    polish = 0.0f;
    telemetry = {};
}

void SealVoice::deactivate() noexcept
{
    resetDsp();
    active = false;
    releasing = false;
}

void SealVoice::hardReset() noexcept
{
    amplitudeEnvelope.reset();
    stolenTail = 0.0f;
    stolenTailSamples = 0;
    noteAgeSeconds = 0.0f;
    deactivate();
}

void SealVoice::startNote(int midiChannel, int midiNoteNumber, float velocity,
                          int currentPitchWheelPosition)
{
    resetDsp();
    currentMidiChannel = midiChannel;
    currentMidiNote = midiNoteNumber;
    currentVelocity = clamp(0.0f, 1.0f, velocity);
    personality = VoicePersonality::create(random);
    preset = &characterPreset(macros.character);
    wobble.prepare(sampleRate, preset->wobbleRateHz, &random);
    flutterDrift.prepare(sampleRate, 7.0f, &random);
    mutter.prepare(sampleRate, 7.0f, &random);
    muffled = 0.0f;
    muffleAmount = macros.behaviorMode == BehaviourMode::murmur ? 1.0f : 0.0f;
    flutterPhase = random.nextFloat();
    thumpPhase = 0.0;
    // BARK scales the punch and the kick, 0.15x to 2.6x (1x at the default 0.35).
    barkScale = 0.15f + 2.43f * macros.bark;
    vibratoPhase = 0.0;
    behaviour.start(currentVelocity, macros, personality, preset->onsetPunch * barkScale);

    baseFrequency = midiNoteToHz(static_cast<float>(currentMidiNote));
    fofBank.setNoteFrequency(baseFrequency);
    noteAgeSeconds = 0.0f;
    pitchWheelMoved(currentPitchWheelPosition);

    // ADSR comes from the character preset. BARK stretches or tightens the
    // attack: twice as slow at 0, a quarter at 1, as measured at the default.
    AdsrEnvelope::Parameters envelope;
    envelope.attack = preset->attackSeconds * std::exp2((0.35f - macros.bark) * 3.0f);
    envelope.decay = preset->decaySeconds;
    envelope.sustain = preset->sustainLevel;
    envelope.release = preset->releaseSeconds;
    amplitudeEnvelope.setParameters(envelope);
    amplitudeEnvelope.noteOn();

    telemetry.velocity = currentVelocity;
    telemetry.registerPosition = clamp(0.0f, 1.0f, (currentMidiNote - 36.0f) / 60.0f);
    telemetry.age = ++ageCounter;
    telemetry.active = true;
    active = true;
    releasing = false;
}

void SealVoice::stopNote(bool allowTailOff)
{
    if (! active)
        return;

    if (allowTailOff)
    {
        amplitudeEnvelope.noteOff();
        behaviour.noteOff();
        releasing = true;
        return;
    }

    stolenTail = previousOutput;
    stolenTailSamples = 32;
    amplitudeEnvelope.reset();
    deactivate();
}

void SealVoice::pitchWheelMoved(int value) noexcept
{
    pitchWheelSemitones = lerp(clamp(0.0f, 16383.0f, static_cast<float>(value)) / 16383.0f,
                               -2.0f, 2.0f);
}

void SealVoice::renderNextBlock(AudioBuffer& output, int startSample, int numSamples)
{
    if (! active)
        return;

    const auto dt = static_cast<float>(1.0 / sampleRate);
    const auto channelCount = output.getNumChannels();

    for (int offset = 0; offset < numSamples; ++offset)
    {
        const MacroState sampleMacros {
            smoothBoom.getNextValue(), smoothAir.getNextValue(), smoothBark.getNextValue(),
            smoothVowel.getNextValue(), macros.space, smoothTide.getNextValue(),
            smoothDetune.getNextValue(), macros.character, macros.behaviorMode
        };

        const auto envelope = amplitudeEnvelope.getNextSample();
        auto vocal = behaviour.process(dt, sampleMacros, envelope, 0.0f);

        // The onset pitch gesture comes from the character preset table, which
        // holds the contour measured from the reference recordings. It settles
        // to its last value and holds, so a sustained note stays in tune.
        noteAgeSeconds += dt;
        // TIDE is expression: 0 plays a dead-straight note, the default 0.25 the
        // measured call, 1 more than twice as much movement plus a vibrato.
        const auto tide = sampleMacros.tide;
        const auto expression = tide < 0.25f ? tide * 4.0f : 1.0f + (tide - 0.25f) * 1.6f;
        const auto gestureLength = preset->gestureSeconds > 0.0f ? preset->gestureSeconds : 0.25f;
        const auto contourSemitones = pitchContourAt(preset->pitchContour,
                                                     noteAgeSeconds / gestureLength)
                                    * preset->gestureDepth * 0.74f * expression;
        vocal.pitchLift = clamp(0.0f, 1.0f, (contourSemitones + 2.0f) * 0.25f);

        const auto detune = detuneOffsetSemitones * sampleMacros.detune * 0.35f;
        // The measured calls never hold a pitch: a smoothed random drift on top
        // of the contour, deepest on SQUEAL.
        auto drift = wobble.next() * preset->wobbleSemitones * expression;
        // GROAN's flutter: about 12.5 Hz, its rate and depth drifting a little.
        if (preset->flutterSemitones > 0.0f)
        {
            const auto d = flutterDrift.next();
            flutterPhase += preset->flutterRateHz * (1.0f + 0.30f * d) * dt;
            flutterPhase -= std::floor(flutterPhase);
            drift += static_cast<float>(std::sin(twoPi<double> * flutterPhase))
                   * preset->flutterSemitones * (0.75f + 0.55f * d) * std::min(1.0f, expression);
        }
        if (tide > 0.4f)    // a singer's vibrato, 5.5 Hz, fading in after the attack
        {
            vibratoPhase += 5.5 * dt;
            vibratoPhase -= std::floor(vibratoPhase);
            const auto fadeIn = clamp(0.0f, 1.0f, (noteAgeSeconds - 0.25f) / 0.35f);
            drift += static_cast<float>(std::sin(twoPi<double> * vibratoPhase))
                   * 0.45f * (tide - 0.4f) / 0.6f * fadeIn;
        }

        // The behaviour mode keeps moving the held note (see modeMotion).
        const auto motion = modeMotion(sampleMacros.behaviorMode, noteAgeSeconds);
        vocal.vowelMorph = clamp(0.0f, 1.0f, vocal.vowelMorph + motion.vowel + preset->vowelOffset);
        auto modeGain = motion.gain;
        if (motion.muffle > 0.0f)    // MURMUR mutters: random syllables, about seven a second
            modeGain *= 0.15f + 0.85f * clamp(0.0f, 1.0f, 0.55f + 0.9f * mutter.next());
        vocal.barkTransient += motion.kick * barkScale;
        drift += motion.semitones;
        const auto frequency = static_cast<double>(baseFrequency)
            * std::pow(2.0f, (contourSemitones + pitchWheelSemitones + detune + drift) / 12.0f);

        auto value = fofBank.process(frequency, *preset, sampleMacros, vocal, random);
        const auto attackPunch = 1.0f + 1.20f * vocal.barkTransient;
        // Headroom: one note at full velocity peaks near -10 dBFS, so eight
        // voices sum to just under full scale and the limiter only catches the
        // coherent worst case rather than working on every chord.
        value *= envelope * vocal.amplitudeShape * attackPunch * modeGain
               * (0.05f + 0.13f * currentVelocity);

        // The kick: a short sine that drops from twice the note to the note in
        // about 15 ms and dies in about 40, under the attack only. It starts at
        // zero phase, so it adds a hit without a click.
        if (preset->thump > 0.0f && noteAgeSeconds < 0.25f)
        {
            const auto sweep = 1.0f + std::exp(-noteAgeSeconds / 0.015f);
            thumpPhase += baseFrequency * sweep * dt;
            thumpPhase -= std::floor(thumpPhase);
            value += static_cast<float>(std::sin(twoPi<double> * thumpPhase))
                   * preset->thump * barkScale * std::exp(-noteAgeSeconds / 0.040f)
                   * (0.05f + 0.13f * currentVelocity) * 1.6f;
        }

        polish += polishCoefficient * (value - polish);
        value = polish;
        // MURMUR's closed mouth: a second, much lower roll-off (about 900 Hz), eased in
        muffleAmount += 0.002f * (motion.muffle - muffleAmount);
        muffled += muffleCoefficient * (value - muffled);
        value = lerp(muffleAmount, value, muffled * 1.6f);

        if (stolenTailSamples > 0)
        {
            value += stolenTail * static_cast<float>(stolenTailSamples) / 32.0f;
            --stolenTailSamples;
        }

        if (! std::isfinite(value))
        {
            value = 0.0f;
            resetDsp();
        }

        previousOutput = value;
        const auto sampleIndex = startSample + offset;
        if (channelCount > 1)
        {
            output.addSample(0, sampleIndex, value * panLeft);
            output.addSample(1, sampleIndex, value * panRight);
            for (int channel = 2; channel < channelCount; ++channel)
                output.addSample(channel, sampleIndex, value);
        }
        else if (channelCount == 1)
        {
            output.addSample(0, sampleIndex, value);
        }

        telemetry.vocal = vocal;
        telemetry.envelope = envelope;
        telemetry.active = true;
    }

    if (! amplitudeEnvelope.isActive())
        deactivate();
}
}
