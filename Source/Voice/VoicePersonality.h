#pragma once

#include "../Core/Random.h"
#include "../Core/PhoqerTypes.h"

namespace phoqer
{
struct VoicePersonality
{
    float pitchOvershoot = 3.0f;
    float pitchDrift = 0.15f;
    PitchGestureType pitchGestureType = PitchGestureType::fall;
    float pitchGestureDepth = 1.0f;
    float gestureSelector = 0.0f;
    float vowelOffset = 0.0f;
    float vowelTravel = 0.2f;
    float mouthOpeningBias = 0.0f;
    float formantScale = 1.0f;
    float formantOffset = 0.0f;
    float throatDrive = 1.0f;
    float throatBias = 1.0f;
    float airiness = 1.0f;
    float airBias = 1.0f;
    float barkVariation = 1.0f;
    float barkBias = 1.0f;
    float movementSpeed = 1.0f;
    float releaseDroop = -1.0f;

    static VoicePersonality create(Random& random) noexcept
    {
        VoicePersonality result;
        result.pitchOvershoot = random.range(1.0f, 7.5f);
        result.pitchDrift = random.range(0.07f, 0.42f);
        result.pitchGestureDepth = random.range(0.72f, 1.32f);
        result.gestureSelector = random.nextFloat();
        result.vowelOffset = random.range(-0.065f, 0.065f);
        result.vowelTravel = random.range(0.13f, 0.34f);
        result.mouthOpeningBias = random.range(-0.11f, 0.12f);
        result.formantScale = random.range(0.90f, 1.10f);
        result.formantOffset = random.range(-110.0f, 110.0f);
        result.throatDrive = random.range(0.84f, 1.20f);
        result.throatBias = random.range(0.86f, 1.16f);
        result.airiness = random.range(0.70f, 1.30f);
        result.airBias = random.range(0.78f, 1.24f);
        result.barkVariation = random.range(0.78f, 1.24f);
        result.barkBias = random.range(0.82f, 1.18f);
        result.movementSpeed = random.range(0.74f, 1.28f);
        result.releaseDroop = random.range(-4.0f, -0.1f);
        return result;
    }
};
}
