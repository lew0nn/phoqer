#pragma once

#include "../Core/DspPrimitives.h"
#include "../Core/PhoqerTypes.h"
#include <algorithm>
#include <cmath>

namespace phoqer::gestures
{
// What each behaviour does to a held note over time: a gain, a pitch offset in
// semitones, a push on the vowel, and an extra bark kick. CALL is one call;
// the others keep moving for as long as the key is held.
struct ModeMotion
{
    float gain = 1.0f, semitones = 0.0f, vowel = 0.0f, kick = 0.0f;
    float muffle = 0.0f;    // 0 open, 1 a closed-mouth hum (MURMUR)
};

inline ModeMotion modeMotion(BehaviourMode mode, double beats, float bpm, float wailBend, float wailHold) noexcept
{
    ModeMotion m;
    const auto beatHz = bpm / 60.0f;      // the modes keep time with the song
    const auto cycle = [beats, beatHz](float multiple)
    {
        const auto p = beats * multiple;
        return static_cast<float>(wrapPhase(p) / (beatHz * multiple));
    };
    switch (mode)
    {
        case BehaviourMode::honk:      // separate honks on 1/8 notes: a nasal hit that drops away to near silence
        {
            const auto c = cycle(2.0f);
            const auto hit = fastExp(-c / 0.07f) * std::min(1.0f, c / 0.006f);
            m.gain = 0.05f + 0.95f * hit;
            m.semitones = 2.5f * fastExp(-c / 0.04f);
            m.vowel = -0.25f + 0.20f * fastExp(-c / 0.06f);
            m.kick = 0.30f * fastExp(-c / 0.025f);
            m.muffle = 0.45f;                                 // half-closed: a nasal honk, not a bark
            break;
        }
        case BehaviourMode::bark:      // separate hard barks on 1/4 notes, silence between
        {
            const auto c = cycle(1.0f);
            // a fast rise, a full 60 ms, then the fall: a bark, not a punch
            const auto open = std::min(1.0f, c / 0.006f) * (c < 0.06f ? 1.0f : fastExp(-(c - 0.06f) / 0.08f));
            const auto shut = 1.0f - std::min(1.0f, std::max(0.0f, (c - 0.16f) / 0.04f));
            m.gain = 0.03f + 0.97f * open * shut;
            m.semitones = 2.5f * fastExp(-c / 0.04f);
            m.kick = 0.25f * fastExp(-c / 0.025f);
            m.vowel = 0.15f;                                  // wide open: a bark, not a honk
            break;
        }
        case BehaviourMode::wail:      // a long glide up and back over two bars; WAIL BEND sets how far (2.4 semitones by default)
        {
            // HOLD: the same rise over the first two beats, then it stays at the top until the note ends.
            // wailHold is the lock eased from 0 to 1, so pressing it during a note glides, never jumps.
            const auto swing = fastSin2Pi(beats / 8.0);
            const auto held = beats >= 2.0 ? 1.0f : swing;
            const auto s = wailHold <= 0.0f ? swing : wailHold >= 1.0f ? held : swing + (held - swing) * wailHold;
            m.semitones = wailBend * s;
            m.vowel = 0.35f * std::max(0.0f, s);              // the mouth opens as it climbs
            m.gain = 0.90f + 0.25f * std::max(0.0f, s);       // and it sings out at the top
            break;
        }
        case BehaviourMode::murmur:    // a closed-mouth "mmm"; the voice adds the muttered syllables
            m.vowel = -0.35f;
            m.muffle = 1.0f;
            break;
        case BehaviourMode::call:
            break;
    }
    return m;
}

// Overfed: a "glk" gulp, then a long rattling belch an octave down that sags as it goes.
inline constexpr float belchSeconds = 1.75f;

inline float belchPitch(float t) noexcept
{
    if (t < 0.3f) return 12.0f - 9.0f * std::min(1.0f, t / 0.12f);           // the gulp, at the fed pitch
    return -4.0f * (t - 0.3f) / 1.45f;                                          // the belch sags
}

inline float belchGain(float t) noexcept
{
    if (t < 0.18f) return 0.8f;
    if (t < 0.3f) return 0.05f;                                                 // a breath before it comes up
    const auto rattle = 0.55f + 0.45f * std::sin(6.2831853f * 21.0f * t);       // the burp's rattle
    return 1.4f * std::min(1.0f, (t - 0.3f) / 0.08f) * std::max(0.0f, 1.0f - (t - 0.3f) / 1.45f) * rattle;
}
}
