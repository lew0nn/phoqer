#pragma once

#include "../Core/PhoqerTypes.h"
#include "../Voice/VoicePersonality.h"

namespace phoqer
{
struct BehaviourProfile
{
    float attackSeconds = 0.018f;
    float releaseSeconds = 0.32f;
    float overshootScale = 1.0f;
    float settleSeconds = 0.08f;
    float mouthStart = 0.18f;
    float mouthSustain = 0.62f;
    float noiseRatio = 0.08f;
};

class BehaviourEngine
{
public:
    static constexpr BehaviourProfile callProfile() noexcept { return {}; }

    void start(float newVelocity, const MacroState& newMacros, const VoicePersonality& personality) noexcept
    {
        velocity = newVelocity;
        voicePersonality = personality;
        barkAmount = juce::jlimit(0.0f, 1.0f,
            std::pow(newMacros.bark, 0.72f) * (0.28f + 0.72f * velocity)
            * personality.barkVariation * personality.barkBias);
        callDuration = juce::jlimit(0.75f, 2.8f,
            (1.75f - 0.65f * barkAmount) / personality.movementSpeed);
        elapsed = 0.0f;
        releaseElapsed = 0.0f;
        releaseStartPhase = 0.0f;
        release = false;
        current = {};
        current.vowelMorph = juce::jlimit(0.0f, 1.0f, newMacros.vowel + personality.vowelOffset);
        current.mouthRound = 1.0f - current.vowelMorph;
        current.amplitudeShape = 1.0f;
    }

    void noteOff() noexcept
    {
        release = true;
        releaseElapsed = 0.0f;
        releaseStartPhase = current.callPhase;
    }

    VocalState process(float dt, const MacroState& macros, float envelope, float movement) noexcept
    {
        elapsed += dt;
        if (release)
            releaseElapsed += dt;

        const auto heldPhase = juce::jmin(0.84f, 0.84f * elapsed / callDuration);
        const auto releaseProgress = smoothStep(0.0f, 0.34f, releaseElapsed);
        const auto callPhase = release
            ? juce::jmap(releaseProgress, releaseStartPhase, 1.0f)
            : heldPhase;
        const auto opening = smoothStep(0.035f, 0.22f, callPhase);
        const auto settling = smoothStep(0.56f, 0.84f, callPhase);
        const auto tail = smoothStep(0.84f, 1.0f, callPhase);
        const auto openPlateau = opening * (1.0f - 0.68f * settling);
        const auto onsetClosed = 1.0f - smoothStep(0.02f, 0.18f, callPhase);

        const auto barkDecay = std::exp(-elapsed / (0.025f + 0.055f * (1.0f - barkAmount)));
        const auto barkTransient = barkAmount * barkDecay;
        const auto evolutionDepth = 0.07f + 0.93f * macros.tide;

        const auto baseVowel = macros.vowel + voicePersonality.vowelOffset;
        const auto openTravel = voicePersonality.vowelTravel * openPlateau;
        const auto darkSettle = (0.055f + 0.12f * voicePersonality.vowelTravel) * settling;
        const auto barkDisplacement = barkAmount * (-0.15f * onsetClosed + 0.10f * openPlateau);
        const auto vowelTarget = juce::jlimit(0.0f, 1.0f,
            baseVowel + evolutionDepth * (openTravel - darkSettle - 0.08f * tail
            + 0.022f * movement * macros.tide) + barkDisplacement);
        const auto vowelRate = 1.0f - std::exp(-dt * (8.0f + 7.0f * barkAmount));
        current.vowelMorph += (vowelTarget - current.vowelMorph) * vowelRate;

        const auto articulationDepth = 0.48f + 0.52f * macros.tide;
        const auto openTarget = juce::jlimit(0.0f, 1.0f,
            0.10f + articulationDepth * (0.72f * openPlateau - 0.24f * tail)
            + voicePersonality.mouthOpeningBias + 0.38f * barkTransient
            + 0.025f * movement * macros.tide);
        const auto openingRate = 7.0f + 28.0f * barkAmount + 3.0f * voicePersonality.movementSpeed;
        current.mouthOpen += (openTarget - current.mouthOpen) * juce::jmin(1.0f, openingRate * dt);
        current.mouthRound = juce::jlimit(0.0f, 1.0f,
            0.90f - 0.74f * current.vowelMorph + 0.12f * macros.boom
            + 0.13f * settling - 0.20f * barkTransient);
        current.throatPressure = juce::jlimit(0.0f, 1.0f,
            (0.16f + 0.42f * envelope + 0.68f * barkTransient + 0.16f * settling
            + 0.11f * macros.boom + 0.11f * macros.tide * movement)
            * voicePersonality.throatBias);
        current.amplitudeShape = juce::jlimit(0.62f, 1.16f,
            0.82f + 0.20f * openPlateau - 0.10f * settling - 0.18f * tail
            + 0.12f * barkTransient + 0.045f * movement * macros.tide);
        current.callIntensity = juce::jlimit(0.0f, 1.0f,
            envelope * current.amplitudeShape * (0.42f + 0.58f * velocity));
        current.barkTransient = barkTransient;
        current.callPhase = juce::jlimit(0.0f, 1.0f, callPhase);
        return current;
    }

    float getBarkAmount() const noexcept { return barkAmount; }

private:
    static float smoothStep(float start, float end, float value) noexcept
    {
        const auto x = juce::jlimit(0.0f, 1.0f, (value - start) / (end - start));
        return x * x * (3.0f - 2.0f * x);
    }

    VocalState current;
    VoicePersonality voicePersonality;
    float elapsed = 0.0f, releaseElapsed = 0.0f, releaseStartPhase = 0.0f;
    float callDuration = 1.5f, velocity = 0.0f, barkAmount = 0.0f;
    bool release = false;
};
}
