#include "SealVoice.h"

#include <cmath>

namespace phoqer
{
SealVoice::SealVoice(uint32_t seed, uint64_t& globalAgeCounter) noexcept
    : random(seed), ageCounter(globalAgeCounter)
{
}

void SealVoice::prepare(double newSampleRate, int)
{
    sampleRate = newSampleRate;
    amplitudeEnvelope.setSampleRate(sampleRate);
    pitchGesture.prepare(sampleRate, &random);
    exciter.prepare(sampleRate);
    throat.prepare(sampleRate);
    formants.prepare(sampleRate);
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
    exciter.reset();
    throat.reset();
    formants.reset();
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
    behaviour.start(currentVelocity, macros, personality);

    const auto barkAmount = behaviour.getBarkAmount();
    pitchGesture.start(currentMidiNote, currentVelocity, macros, personality, barkAmount);
    pitchWheelMoved(currentPitchWheelPosition);

    AdsrEnvelope::Parameters envelope;
    envelope.attack = lerp(barkAmount, 0.0060f, 0.0015f);
    envelope.decay = lerp(barkAmount, 0.32f, 0.18f);
    envelope.sustain = lerp(barkAmount, 0.78f, 0.62f);
    envelope.release = lerp(barkAmount, 0.26f, 0.14f);
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
        pitchGesture.noteOff();
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
    const auto bendMultiplier = std::pow(2.0f, pitchWheelSemitones / 12.0f);

    for (int offset = 0; offset < numSamples; ++offset)
    {
        const MacroState sampleMacros {
            smoothBoom.getNextValue(), smoothAir.getNextValue(), smoothBark.getNextValue(),
            smoothVowel.getNextValue(), macros.space, smoothTide.getNextValue(),
            smoothDetune.getNextValue()
        };
        if (behaviour.isFinished())
        {
            deactivate();
            break;
        }
        const auto envelope = amplitudeEnvelope.getNextSample();
        auto vocal = behaviour.process(dt, sampleMacros, envelope, 0.0f);
        const auto frequency = pitchGesture.nextFrequency(sampleMacros, vocal.callPhase) * bendMultiplier;
        vocal.pitchLift = pitchGesture.getPitchLift();
        const auto excitation = exciter.process(frequency, sampleMacros, vocal, personality, random);
        const auto pressured = throat.process(excitation, sampleMacros, vocal, personality);
        auto value = formants.process(pressured, static_cast<float>(currentMidiNote), sampleMacros,
                                      vocal, personality, 0.0f);
        const auto attackPunch = 1.0f + 1.20f * vocal.barkTransient;
        value *= envelope * vocal.amplitudeShape * attackPunch
               * (0.18f + 0.22f * currentVelocity);

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
        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            output.addSample(channel, sampleIndex, value);

        telemetry.vocal = vocal;
        telemetry.envelope = envelope;
        telemetry.active = true;
    }

    if (! amplitudeEnvelope.isActive())
        deactivate();
}
}
