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
        result.pitchOvershoot = 1.0f;
        result.pitchDrift = 0.0f;
        result.pitchGestureDepth = 1.0f;
        result.gestureSelector = 0.0f;
        result.vowelOffset = 0.0f;
        result.vowelTravel = 0.24f;
        result.mouthOpeningBias = 0.0f;
        result.formantScale = 1.0f;
        result.formantOffset = 0.0f;
        result.throatDrive = 1.0f;
        result.throatBias = random.range(0.97f, 1.03f);
        result.airiness = random.range(0.94f, 1.06f);
        result.airBias = random.range(0.96f, 1.04f);
        result.barkVariation = random.range(0.95f, 1.05f);
        result.barkBias = random.range(0.97f, 1.03f);
        result.movementSpeed = 1.0f;
        result.releaseDroop = -0.65f;
        return result;
    }
};
}
