#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/DspPrimitives.h"
#include "../Core/PhoqerTypes.h"
#include "../Core/Random.h"
#include "../Core/CharacterPreset.h"
#include "../DSP/BehaviourEngine.h"
#include "../DSP/FofBank.h"
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
    FofBank fofBank;
    AdsrEnvelope amplitudeEnvelope;
    LinearSmoother smoothBoom, smoothAir, smoothBark, smoothVowel, smoothTide, smoothDetune;
    VoiceTelemetry telemetry;
    const CharacterPreset* preset = &characterPreset(defaultSealCharacter);
    double sampleRate = 44100.0;
    float baseFrequency = 261.63f;
    float noteAgeSeconds = 0.0f;
    float detuneOffsetSemitones = 0.0f;
    float panLeft = 0.70710678f;
    float panRight = 0.70710678f;
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
