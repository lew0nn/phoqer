#include "SealVoice.h"

namespace phoqer
{
SealVoice::SealVoice(uint32_t seed, uint64_t& globalAgeCounter) noexcept
    : random(seed), ageCounter(globalAgeCounter)
{
}

bool SealVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SealSound*>(sound) != nullptr;
}

void SealVoice::prepare(double newSampleRate, int)
{
    sampleRate = newSampleRate;
    amplitudeEnvelope.setSampleRate(sampleRate);
    pitchGesture.prepare(sampleRate, &random);
    exciter.prepare(sampleRate);
    throat.prepare(sampleRate);
    formants.prepare(sampleRate);
    tideMovement.prepare(sampleRate, 1.7f, &random);
    for (auto* smoother : { &smoothBoom, &smoothAir, &smoothBark, &smoothVowel, &smoothTide })
        smoother->reset(sampleRate, 0.035);
    smoothBoom.setCurrentAndTargetValue(macros.boom);
    smoothAir.setCurrentAndTargetValue(macros.air);
    smoothBark.setCurrentAndTargetValue(macros.bark);
    smoothVowel.setCurrentAndTargetValue(macros.vowel);
    smoothTide.setCurrentAndTargetValue(macros.tide);
    resetDsp();
}

void SealVoice::setMacros(const MacroState& newMacros) noexcept
{
    macros = newMacros;
    smoothBoom.setTargetValue(macros.boom);
    smoothAir.setTargetValue(macros.air);
    smoothBark.setTargetValue(macros.bark);
    smoothVowel.setTargetValue(macros.vowel);
    smoothTide.setTargetValue(macros.tide);
}

void SealVoice::resetDsp() noexcept
{
    exciter.reset();
    throat.reset();
    formants.reset();
    tideMovement.reset();
    previousOutput = 0.0f;
    telemetry = {};
}

void SealVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*,
                          int currentPitchWheelPosition)
{
    resetDsp();
    currentMidiNote = midiNoteNumber;
    currentVelocity = juce::jlimit(0.0f, 1.0f, velocity);
    personality = VoicePersonality::create(random);
    behaviour.start(currentVelocity, macros, personality);

    const auto barkAmount = behaviour.getBarkAmount();
    pitchGesture.start(currentMidiNote, currentVelocity, macros, personality, barkAmount);
    pitchWheelMoved(currentPitchWheelPosition);

    juce::ADSR::Parameters envelope;
    envelope.attack = juce::jmap(barkAmount, 0.035f, 0.0018f);
    envelope.decay = juce::jmap(barkAmount, 0.16f, 0.055f);
    envelope.sustain = juce::jmap(barkAmount, 0.84f, 0.62f);
    envelope.release = juce::jmap(barkAmount, 0.46f, 0.13f);
    amplitudeEnvelope.setParameters(envelope);
    amplitudeEnvelope.noteOn();

    telemetry.velocity = currentVelocity;
    telemetry.registerPosition = juce::jlimit(0.0f, 1.0f, (currentMidiNote - 36.0f) / 60.0f);
    telemetry.age = ++ageCounter;
    telemetry.active = true;
    releasing = false;
}

void SealVoice::stopNote(float, bool allowTailOff)
{
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
    resetDsp();
    clearCurrentNote();
    releasing = false;
}

void SealVoice::pitchWheelMoved(int value)
{
    pitchWheelSemitones = juce::jmap(static_cast<float>(value), 0.0f, 16383.0f, -2.0f, 2.0f);
}

void SealVoice::renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const auto dt = static_cast<float>(1.0 / sampleRate);
    const auto bendMultiplier = std::pow(2.0f, pitchWheelSemitones / 12.0f);

    for (int offset = 0; offset < numSamples; ++offset)
    {
        const MacroState sampleMacros {
            smoothBoom.getNextValue(), smoothAir.getNextValue(), smoothBark.getNextValue(),
            smoothVowel.getNextValue(), macros.space, smoothTide.getNextValue()
        };
        const auto envelope = amplitudeEnvelope.getNextSample();
        const auto movement = tideMovement.next();
        auto vocal = behaviour.process(dt, sampleMacros, envelope, movement);
        const auto frequency = pitchGesture.nextFrequency(sampleMacros, vocal.callPhase) * bendMultiplier;
        vocal.pitchLift = pitchGesture.getPitchLift();
        const auto excitation = exciter.process(frequency, sampleMacros, vocal, personality, random);
        const auto pressured = throat.process(excitation, sampleMacros, vocal, personality);
        auto value = formants.process(pressured, static_cast<float>(currentMidiNote), sampleMacros,
                                      vocal, personality, movement);
        const auto amplitudeWander = 1.0f + movement * (0.02f + sampleMacros.tide * 0.14f);
        value *= envelope * vocal.amplitudeShape * amplitudeWander * (0.16f + 0.18f * currentVelocity);

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
    {
        resetDsp();
        clearCurrentNote();
        releasing = false;
    }
}
}
