#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/DspPrimitives.h"
#include "../Core/PhoqerTypes.h"
#include "../Core/Random.h"
#include "../DSP/BehaviourEngine.h"
#include "../DSP/Exciter.h"
#include "../DSP/FormantBank.h"
#include "../DSP/PitchGesture.h"
#include "../DSP/ThroatStage.h"
#include "VoicePersonality.h"

namespace phoqer
{
class SealVoice final
{
public:
    SealVoice(uint32_t seed, uint64_t& globalAgeCounter) noexcept;

    void prepare(double sampleRate, int maximumBlockSize);
    void hardReset() noexcept;
    void setMacros(const MacroState& newMacros) noexcept;
    void startNote(int midiChannel, int midiNoteNumber, float velocity, int currentPitchWheelPosition);
    void stopNote(bool allowTailOff);
    void pitchWheelMoved(int newPitchWheelValue) noexcept;
    void renderNextBlock(AudioBuffer& output, int startSample, int numSamples);

    bool isActive() const noexcept { return active; }
    bool matchesNote(int midiChannel, int midiNoteNumber) const noexcept
    {
        return active && currentMidiChannel == midiChannel && currentMidiNote == midiNoteNumber;
    }
    bool matchesChannel(int midiChannel) const noexcept { return active && currentMidiChannel == midiChannel; }
    const VoiceTelemetry& getTelemetry() const noexcept { return telemetry; }
    float getEnvelopeLevel() const noexcept { return telemetry.envelope; }
    bool isReleasing() const noexcept { return releasing; }
    uint64_t getAge() const noexcept { return telemetry.age; }

private:
    void resetDsp() noexcept;
    void deactivate() noexcept;

    Random random;
    uint64_t& ageCounter;
    VoicePersonality personality;
    MacroState macros;
    BehaviourEngine behaviour;
    PitchGesture pitchGesture;
    Exciter exciter;
    ThroatStage throat;
    FormantBank formants;
    AdsrEnvelope amplitudeEnvelope;
    LinearSmoother smoothBoom, smoothAir, smoothBark, smoothVowel, smoothTide, smoothDetune;
    VoiceTelemetry telemetry;
    double sampleRate = 44100.0;
    int currentMidiChannel = 0;
    int currentMidiNote = 60;
    float currentVelocity = 0.0f;
    float pitchWheelSemitones = 0.0f;
    float previousOutput = 0.0f;
    float stolenTail = 0.0f;
    int stolenTailSamples = 0;
    bool active = false;
    bool releasing = false;
};
}
