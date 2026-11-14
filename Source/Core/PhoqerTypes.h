#pragma once

#include "SealCharacter.h"

#include <array>
#include <cstdint>

namespace phoqer
{
struct CharacterPreset;
constexpr int voiceCount = 8;
constexpr int nitroNotes = 3;     // NITRO adds up to three notes to every key

// NITRO (KYRAX only): every key also plays up to three more notes, each its own voice. An offset of 0
// leaves that slot empty. The group is level-matched, so NITRO changes the sound, not the loudness.
struct NitroSettings
{
    bool on = false;
    std::array<int, nitroNotes> offsets { 7, 12, 0 };              // semitones from the key
    std::array<float, nitroNotes> levels { 0.7f, 0.7f, 0.7f };      // against the key's own note
    float width = 0.6f;          // how far the extra notes spread left and right, 0..1
    float strumMs = 12.0f;       // each extra note starts this much after the one before
    float detuneCents = 0.0f;    // the extra notes pushed sharp and flat in turn
};

// ARP (KYRAX only): the held keys are played one at a time at the tempo instead of together.
struct ArpSettings
{
    bool on = false;
    int rate = 2;               // 0 1/4, 1 1/8, 2 1/16, 3 1/32, 4 1/8 triplets
    int pattern = 0;            // 0 UP, 1 DOWN, 2 UP + DOWN, 3 RANDOM, 4 AS PLAYED
    bool hold = false;          // keeps playing after the keys are let go
    float gate = 0.6f;          // each note's length, as a share of its step
    float swing = 0.0f;         // every second step pushed late, 0..1
    int octaves = 1;            // the held keys, then again an octave up, up to 3
};

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
    float detune = 0.0f;
    SealCharacter character = defaultSealCharacter;
    BehaviourMode behaviorMode = BehaviourMode::call;
    float tempoBpm = 120.0f;     // the host's tempo; HONK, BARK and WAIL keep time with it
    bool songPlaying = false;    // the host is playing: ARP and YAP lock to the song's beat grid
    double songBeats = 0.0;      // where this block starts in the song, in quarter notes (while songPlaying)
    float glideMs = 0.0f;        // a note played over a held one slides in from its pitch
    float wailBend = 2.4f;       // how far WAIL sweeps up and down, in semitones
    bool wailHold = false;       // WAIL rises to the bend and stays there instead of swinging
    const CharacterPreset* loadedVoice = nullptr;
    bool yap = false;            // KYRAX only: VOWEL steps through a talking pattern in 8ths at the tempo
    NitroSettings nitro;         // KYRAX only
    ArpSettings arp;             // KYRAX only
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
