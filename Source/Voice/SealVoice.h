#pragma once

#include "../Core/PhoqerTypes.h"
#include "../Core/Random.h"
#include "../DSP/BehaviourEngine.h"
#include "../DSP/Exciter.h"
#include "../DSP/FormantBank.h"
#include "../DSP/PitchGesture.h"
#include "../DSP/SmoothedRandom.h"
#include "../DSP/ThroatStage.h"
#include "SealSound.h"
#include "VoicePersonality.h"
#include <JuceHeader.h>

namespace phoqer
{
class SealVoice final : public juce::SynthesiserVoice
{
public:
    SealVoice(uint32_t seed, uint64_t& globalAgeCounter) noexcept;

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void prepare(double sampleRate, int maximumBlockSize);
    void setMacros(const MacroState& newMacros) noexcept;

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*,
                   int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override;

    const VoiceTelemetry& getTelemetry() const noexcept { return telemetry; }
    float getEnvelopeLevel() const noexcept { return telemetry.envelope; }
    bool isReleasing() const noexcept { return releasing; }
    uint64_t getAge() const noexcept { return telemetry.age; }

private:
    void resetDsp() noexcept;

    Random random;
    uint64_t& ageCounter;
    VoicePersonality personality;
    MacroState macros;
    BehaviourEngine behaviour;
    PitchGesture pitchGesture;
    Exciter exciter;
    ThroatStage throat;
    FormantBank formants;
    SmoothedRandom tideMovement;
    juce::ADSR amplitudeEnvelope;
    juce::SmoothedValue<float> smoothBoom, smoothAir, smoothBark, smoothVowel, smoothTide;
    VoiceTelemetry telemetry;
    double sampleRate = 44100.0;
    int currentMidiNote = 60;
    float currentVelocity = 0.0f;
    float pitchWheelSemitones = 0.0f;
    float previousOutput = 0.0f;
    float stolenTail = 0.0f;
    int stolenTailSamples = 0;
    bool releasing = false;
};
}
