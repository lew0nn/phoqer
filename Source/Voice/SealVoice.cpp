#include "SealVoice.h"

#include <cmath>

namespace phoqer
{
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
    behaviour.start(currentVelocity, macros, personality);

    baseFrequency = midiNoteToHz(static_cast<float>(currentMidiNote));
    noteAgeSeconds = 0.0f;
    pitchWheelMoved(currentPitchWheelPosition);

    // ADSR comes from the character preset, so GROAN keeps its 280 ms swell and
    // SQUEAL keeps its 15 ms bark. A hard bark shortens the attack further.
    const auto barkAmount = behaviour.getBarkAmount();
    AdsrEnvelope::Parameters envelope;
    envelope.attack = preset->attackSeconds * lerp(barkAmount, 1.0f, 0.45f);
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
        const auto gestureLength = preset->gestureSeconds > 0.0f ? preset->gestureSeconds : 0.25f;
        const auto contourSemitones = pitchContourAt(preset->pitchContour,
                                                     noteAgeSeconds / gestureLength)
                                    * preset->gestureDepth
                                    * (0.55f + 0.75f * sampleMacros.tide);
        vocal.pitchLift = clamp(0.0f, 1.0f, (contourSemitones + 2.0f) * 0.25f);

        const auto detune = detuneOffsetSemitones * sampleMacros.detune * 0.35f;
        const auto frequency = static_cast<double>(baseFrequency)
            * std::pow(2.0f, (contourSemitones + pitchWheelSemitones + detune) / 12.0f);

        auto value = fofBank.process(frequency, *preset, sampleMacros, vocal, random);
        const auto attackPunch = 1.0f + 1.20f * vocal.barkTransient;
        // Headroom: one note at full velocity peaks near -10 dBFS, so eight
        // voices sum to just under full scale and the limiter only catches the
        // coherent worst case rather than working on every chord.
        value *= envelope * vocal.amplitudeShape * attackPunch
               * (0.05f + 0.13f * currentVelocity);

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
