#pragma once

#include "../Core/PhoqerTypes.h"
#include "../Core/DspPrimitives.h"
#include "../Voice/VoicePersonality.h"

namespace phoqer
{
struct BehaviourProfile
{
    float attackSeconds = 0.006f;
    float releaseSeconds = 0.12f;
    float overshootScale = 1.0f;
    float settleSeconds = 0.08f;
    float mouthStart = 0.12f;
    float mouthSustain = 0.52f;
    float noiseRatio = 0.06f;
};

class BehaviourEngine
{
public:
    static constexpr BehaviourProfile callProfile() noexcept { return {}; }

    void start(float newVelocity, const MacroState& newMacros,
               const VoicePersonality& personality) noexcept
    {
        velocity = newVelocity;
        voicePersonality = personality;
        barkAmount = clamp(0.0f, 1.0f,
            std::pow(newMacros.bark, 0.66f) * (0.34f + 0.66f * velocity)
            * personality.barkVariation * personality.barkBias);
        callDuration = clamp(1.20f, 2.25f,
            1.82f - 0.42f * barkAmount + 0.28f * newMacros.tide);
        elapsed = 0.0f;
        releaseElapsed = 0.0f;
        releaseStartPhase = 0.0f;
        release = false;
        current = {};
        current.vowelMorph = clamp(0.0f, 1.0f, newMacros.vowel);
        current.mouthRound = 1.0f - current.vowelMorph;
        current.amplitudeShape = 0.0f;
    }

    void noteOff() noexcept
    {
        release = true;
        releaseElapsed = 0.0f;
        releaseStartPhase = current.callPhase;
    }

    VocalState process(float dt, const MacroState& macros, float envelope, float) noexcept
    {
        elapsed += dt;
        if (release)
            releaseElapsed += dt;

        const auto naturalPhase = clamp(0.0f, 1.0f, elapsed / callDuration);
        const auto releaseProgress = smoothStep(0.0f, 0.30f, releaseElapsed);
        const auto callPhase = release
            ? std::max(naturalPhase, lerp(releaseProgress, releaseStartPhase, 1.0f))
            : naturalPhase;

        const auto attackGate = smoothStep(0.0f,
            0.0015f + 0.0035f * (1.0f - barkAmount), elapsed);
        const auto tailGate = 1.0f
            - smoothStep(callDuration * 0.84f, callDuration, elapsed);
        const auto barkTransient = (0.48f + 0.52f * barkAmount)
            * std::exp(-elapsed / (0.032f + 0.030f * (1.0f - barkAmount)));

        // Finite chest-driven thrusts, not a continuous LFO. One note is a
        // bark opening into a short groan rather than an indefinitely held pad.
        const auto firstThrust = bellPulse(elapsed, 0.052f, 0.042f);
        const auto secondThrust = bellPulse(elapsed, 0.345f, 0.115f)
                                * (0.58f + 0.34f * macros.tide);
        const auto thirdThrust = bellPulse(elapsed, 0.650f, 0.145f)
                               * (0.12f + 0.46f * macros.tide);
        const auto thrust = clamp(0.0f, 1.0f,
            firstThrust + secondThrust + thirdThrust);

        const auto opening = smoothStep(0.018f, 0.14f, elapsed);
        const auto closing = smoothStep(callDuration * 0.68f,
                                        callDuration * 0.95f, elapsed);
        const auto openPlateau = opening * (1.0f - 0.58f * closing);
        const auto groanTravel = (0.12f + 0.25f * macros.tide)
                               * smoothStep(0.08f, 0.42f, elapsed);
        const auto vowelTarget = clamp(0.0f, 1.0f,
            macros.vowel - 0.12f * barkTransient + groanTravel
            - 0.10f * closing + 0.035f * thrust);
        const auto vowelRate = 1.0f - std::exp(-dt * (10.0f + 12.0f * barkAmount));
        current.vowelMorph += (vowelTarget - current.vowelMorph) * vowelRate;

        const auto openTarget = clamp(0.0f, 1.0f,
            0.07f + 0.66f * openPlateau + 0.22f * thrust
            + 0.30f * barkTransient + voicePersonality.mouthOpeningBias);
        const auto openingRate = 18.0f + 34.0f * barkAmount;
        current.mouthOpen += (openTarget - current.mouthOpen)
                           * std::min(1.0f, openingRate * dt);
        current.mouthRound = clamp(0.0f, 1.0f,
            0.88f - 0.66f * current.vowelMorph + 0.12f * macros.boom
            + 0.08f * closing - 0.18f * barkTransient);

        current.throatPressure = clamp(0.0f, 1.0f,
            (0.18f + 0.18f * envelope + 0.58f * thrust
            + 0.58f * barkTransient + 0.08f * macros.boom)
            * voicePersonality.throatBias);

        const auto groanDecay = std::exp(-elapsed
            / (0.78f + 0.40f * (1.0f - barkAmount)));
        const auto thrustGate = 0.70f + 0.30f * thrust;
        current.amplitudeShape = clamp(0.0f, 1.22f,
            attackGate * tailGate
            * ((0.58f + 0.42f * groanDecay) * thrustGate
               + 0.62f * barkTransient));
        current.callIntensity = clamp(0.0f, 1.0f,
            envelope * current.amplitudeShape * (0.38f + 0.62f * velocity));
        current.barkTransient = barkTransient;
        current.callPhase = callPhase;
        return current;
    }

    float getBarkAmount() const noexcept { return barkAmount; }
    bool isFinished() const noexcept
    {
        return elapsed >= callDuration || (release && releaseElapsed >= 0.30f);
    }

private:
    static float smoothStep(float start, float end, float value) noexcept
    {
        const auto x = clamp(0.0f, 1.0f, (value - start) / (end - start));
        return x * x * (3.0f - 2.0f * x);
    }

    static float bellPulse(float time, float centre, float width) noexcept
    {
        const auto x = (time - centre) / std::max(0.001f, width);
        return std::exp(-0.5f * x * x);
    }

    VocalState current;
    VoicePersonality voicePersonality;
    float elapsed = 0.0f;
    float releaseElapsed = 0.0f;
    float releaseStartPhase = 0.0f;
    float callDuration = 1.1f;
    float velocity = 0.0f;
    float barkAmount = 0.0f;
    bool release = false;
};
}
