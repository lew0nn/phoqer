#pragma once

#include "../Core/DspPrimitives.h"
#include "../Core/PhoqerTypes.h"
#include "../Core/Random.h"
#include "../Voice/VoicePersonality.h"

namespace phoqer
{
class PitchGesture
{
public:
    void prepare(double newSampleRate, Random*) noexcept
    {
        sampleRate = newSampleRate;
    }

    void start(int midiNote, float, const MacroState& macros,
               const VoicePersonality&, float barkAmount) noexcept
    {
        baseFrequency = midiNoteToHz(static_cast<float>(midiNote));
        ageSeconds = 0.0f;
        releaseAge = 0.0f;
        released = false;
        bark = barkAmount;
        if (macros.tide > 0.82f && barkAmount < 0.55f)
            gestureType = PitchGestureType::wail;
        else
            gestureType = PitchGestureType::fall;
        lastPitchLift = 0.0f;
    }

    void noteOff() noexcept
    {
        released = true;
        releaseAge = 0.0f;
    }

    float nextFrequency(const MacroState& macros, float callPhase) noexcept
    {
        const auto dt = static_cast<float>(1.0 / sampleRate);
        ageSeconds += dt;
        if (released)
            releaseAge += dt;

        // One deterministic vocal trajectory: a short high bark collapses into
        // a lower chest groan. There is no random or cyclic pitch modulation.
        const auto onsetSemitones = (0.75f + 2.65f * bark)
                                  * std::exp(-ageSeconds / (0.045f + 0.025f * (1.0f - bark)));
        const auto groanDroop = -(0.38f + 0.82f * macros.boom)
                             * (1.0f - std::exp(-ageSeconds / 0.28f));
        const auto bodyArc = (gestureType == PitchGestureType::wail ? 0.44f : 0.16f)
                           * macros.tide * smoothBell(0.10f, 0.48f, 0.88f, callPhase);
        // After the initial bark falls into the chest, the decay rises through
        // the vowel and note-off lifts again. This keeps the call from ending in
        // the same one-direction downward gesture on every note.
        const auto decayRise = (0.72f + 1.08f * macros.tide)
                             * smoothStep(0.24f, 0.88f, callPhase);
        const auto releaseRise = released
            ? (0.85f + 0.70f * macros.tide)
              * (1.0f - std::exp(-releaseAge / 0.075f)) : 0.0f;
        const auto semitones = clamp(-2.0f, 4.5f,
            onsetSemitones + groanDroop + bodyArc + decayRise + releaseRise);
        lastPitchLift = clamp(0.0f, 1.0f,
            std::max(0.0f, semitones) / 2.4f);
        return baseFrequency * std::pow(2.0f, semitones / 12.0f);
    }

    float getPitchLift() const noexcept { return lastPitchLift; }
    PitchGestureType getGestureType() const noexcept { return gestureType; }

private:
    static float smoothStep(float start, float end, float value) noexcept
    {
        const auto x = clamp(0.0f, 1.0f, (value - start) / (end - start));
        return x * x * (3.0f - 2.0f * x);
    }

    static float smoothBell(float start, float peak, float end, float value) noexcept
    {
        return smoothStep(start, peak, value) * (1.0f - smoothStep(peak, end, value));
    }

    double sampleRate = 44100.0;
    float baseFrequency = 440.0f;
    float ageSeconds = 0.0f;
    float releaseAge = 0.0f;
    float bark = 0.0f;
    float lastPitchLift = 0.0f;
    PitchGestureType gestureType = PitchGestureType::scoop;
    bool released = false;
};
}
