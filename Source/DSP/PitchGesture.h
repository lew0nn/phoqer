#pragma once

#include "SmoothedRandom.h"
#include "../Core/PhoqerTypes.h"
#include "../Voice/VoicePersonality.h"

namespace phoqer
{
class PitchGesture
{
public:
    void prepare(double newSampleRate, Random* random)
    {
        sampleRate = newSampleRate;
        pitchDrift.prepare(sampleRate, 5.5f, random);
        slowWander.prepare(sampleRate, 1.1f, random);
    }

    void start(int midiNote, float velocity, const MacroState& macros,
               const VoicePersonality& newPersonality, float barkAmount) noexcept
    {
        baseFrequency = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNote));
        personality = newPersonality;
        ageSeconds = 0.0f;
        releaseAge = 0.0f;
        released = false;
        selectGesture(macros, velocity, barkAmount);
        overshoot = personality.pitchOvershoot * (0.45f + 0.55f * velocity)
                    * personality.pitchGestureDepth * (0.55f + 0.85f * barkAmount);
        settleSeconds = juce::jlimit(0.028f, 0.17f,
            0.165f - 0.125f * barkAmount + 0.012f * personality.movementSpeed);
        lastPitchLift = 0.0f;
    }

    void noteOff() noexcept { released = true; releaseAge = 0.0f; }

    float nextFrequency(const MacroState& macros, float callPhase) noexcept
    {
        const auto dt = static_cast<float>(1.0 / sampleRate);
        ageSeconds += dt;
        if (released)
            releaseAge += dt;

        const auto attack = std::exp(-ageSeconds / settleSeconds);
        const auto evolutionDepth = 0.12f + 0.88f * macros.tide;
        const auto gestureDepth = personality.pitchGestureDepth * evolutionDepth;
        float gestureSemitones = 0.0f;
        switch (personality.pitchGestureType)
        {
            case PitchGestureType::fall:
                gestureSemitones = overshoot * attack
                                 - 0.28f * gestureDepth * smoothBell(0.22f, 0.46f, 0.70f, callPhase);
                break;
            case PitchGestureType::scoop:
                gestureSemitones = -1.65f * gestureDepth * std::exp(-ageSeconds / 0.11f)
                                 + 0.62f * gestureDepth * smoothBell(0.10f, 0.31f, 0.58f, callPhase);
                break;
            case PitchGestureType::bend:
                gestureSemitones = 2.15f * gestureDepth * smoothBell(0.10f, 0.35f, 0.68f, callPhase)
                                 + 0.10f * overshoot * attack;
                break;
            case PitchGestureType::flat:
                gestureSemitones = 0.11f * overshoot * attack;
                break;
            case PitchGestureType::wail:
                gestureSemitones = (3.2f + 1.8f * macros.tide) * gestureDepth
                                 * (smoothStep(0.08f, 0.50f, callPhase)
                                    - 1.16f * smoothStep(0.55f, 0.86f, callPhase));
                break;
        }

        const auto wander = slowWander.next();
        const auto driftSemitones = pitchDrift.next() * personality.pitchDrift * (0.12f + 1.18f * macros.tide)
                                  + wander * 0.28f * macros.tide;
        const auto wobble = std::sin(ageSeconds * (6.0f + personality.movementSpeed) + wander)
                          * 0.024f * (0.15f + macros.tide);
        const auto droop = released ? personality.releaseDroop * (1.0f - std::exp(-releaseAge / 0.18f)) : 0.0f;
        const auto semitones = juce::jlimit(-6.0f, 9.0f,
            gestureSemitones + driftSemitones + wobble + droop);
        lastPitchLift = juce::jlimit(0.0f, 1.0f, juce::jmax(0.0f, gestureSemitones) / 5.0f);
        return baseFrequency * std::pow(2.0f, semitones / 12.0f);
    }

    float getPitchLift() const noexcept { return lastPitchLift; }
    PitchGestureType getGestureType() const noexcept { return personality.pitchGestureType; }

private:
    void selectGesture(const MacroState& macros, float velocity, float barkAmount) noexcept
    {
        const auto selector = personality.gestureSelector;
        if (barkAmount > 0.62f)
            personality.pitchGestureType = selector < 0.78f ? PitchGestureType::fall
                                                            : PitchGestureType::bend;
        else if (macros.tide > 0.68f)
            personality.pitchGestureType = selector < 0.38f ? PitchGestureType::wail
                                         : selector < 0.66f ? PitchGestureType::bend
                                         : selector < 0.87f ? PitchGestureType::fall
                                                            : PitchGestureType::scoop;
        else if (macros.bark < 0.16f && macros.tide < 0.28f)
            personality.pitchGestureType = selector < 0.30f ? PitchGestureType::flat
                                         : selector < 0.50f ? PitchGestureType::scoop
                                                            : PitchGestureType::fall;
        else
            personality.pitchGestureType = selector < 0.44f ? PitchGestureType::fall
                                         : selector < 0.61f ? PitchGestureType::scoop
                                         : selector < 0.79f ? PitchGestureType::bend
                                         : selector < 0.91f || velocity < 0.35f ? PitchGestureType::flat
                                                                              : PitchGestureType::wail;
    }

    static float smoothStep(float start, float end, float value) noexcept
    {
        const auto x = juce::jlimit(0.0f, 1.0f, (value - start) / (end - start));
        return x * x * (3.0f - 2.0f * x);
    }

    static float smoothBell(float start, float peak, float end, float value) noexcept
    {
        return smoothStep(start, peak, value) * (1.0f - smoothStep(peak, end, value));
    }

    SmoothedRandom pitchDrift, slowWander;
    VoicePersonality personality;
    double sampleRate = 44100.0;
    float baseFrequency = 440.0f, ageSeconds = 0.0f, releaseAge = 0.0f;
    float overshoot = 3.0f, settleSeconds = 0.08f, lastPitchLift = 0.0f;
    bool released = false;
};
}
