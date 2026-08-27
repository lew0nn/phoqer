#pragma once

#include <JuceHeader.h>

namespace phoqer
{
constexpr int voiceCount = 8;

enum class BehaviourMode : uint8_t
{
    call,
    honk,
    bark,
    wail,
    murmur
};

enum class PitchGestureType : uint8_t
{
    fall,
    scoop,
    bend,
    flat,
    wail
};

struct MacroState
{
    float boom = 0.5f;
    float air = 0.25f;
    float bark = 0.35f;
    float vowel = 0.35f;
    float space = 0.2f;
    float tide = 0.25f;
};

struct VocalState
{
    float mouthOpen = 0.0f;
    float mouthRound = 0.5f;
    float throatPressure = 0.0f;
    float callIntensity = 0.0f;
    float barkTransient = 0.0f;
    float vowelMorph = 0.35f;
    float callPhase = 0.0f;
    float amplitudeShape = 1.0f;
    float pitchLift = 0.0f;
};

struct VoiceTelemetry
{
    VocalState vocal;
    float envelope = 0.0f;
    float velocity = 0.0f;
    float registerPosition = 0.0f;
    uint64_t age = 0;
    bool active = false;
};
}
