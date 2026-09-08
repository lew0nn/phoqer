#pragma once

#include "SealCharacter.h"

#include <array>
#include <cstddef>

namespace phoqer
{
// One resonance of the vocal tract. Q is frequency / bandwidth, so a narrow
// bandwidth is a strong, singing formant. GROAN's first formant is measured at
// 1120 Hz with a 34 Hz bandwidth, i.e. Q of about 33, which is why the engine
// uses FOF grains rather than a feedback filter bank.
struct FormantSpec
{
    float frequency = 1000.0f;   // Hz
    float bandwidth = 100.0f;    // Hz
    float gain = 1.0f;           // linear
    // How strongly the centre is pulled onto the nearest harmonic of the played
    // note, from 0 (a fixed formant) to 1 (locked to a harmonic).
    //
    // A very narrow formant only speaks when a harmonic lands inside it. GROAN's
    // formant is 34 Hz wide, so at a 622 Hz fundamental, where the harmonics are
    // 622 and 1244, a fixed 1120 Hz centre falls in the gap and produces almost
    // nothing. The reference recording shows the opposite: its peak sits on the
    // second harmonic. Pulling the centre onto a harmonic is both what the
    // recording does and what keeps a narrow formant audible across the keyboard.
    float harmonicLock = 0.0f;
};

// The onset pitch gesture, sampled at equal positions from note-on to the end
// of the gesture. Values are semitone offsets applied on top of the played note.
inline constexpr std::size_t pitchContourPoints = 8;

// Five bands: a low source/body resonance that carries the 300-800 Hz region,
// then the four measured formants. Without the source band the instrument has a
// hole exactly where the references put most of their energy.
inline constexpr std::size_t formantBandCount = 5;
using PitchContour = std::array<float, pitchContourPoints>;

struct CharacterPreset
{
    const char* id = "";
    const char* displayName = "";

    std::array<FormantSpec, formantBandCount> formants {};

    float attackSkirtSeconds = 0.0015f;  // FOF beta: skirt width, sets brightness
    float noiseMix = 0.05f;              // 0..1, derived from the measured HNR
    float jitter = 0.005f;               // period-to-period pitch wobble, fraction
    float shimmer = 0.05f;               // period-to-period amplitude wobble, fraction
    float subharmonicChance = 0.0f;      // per-period probability of period doubling
    float subOctaveGain = 0.0f;          // sub sine mixed under the fundamental

    float attackSeconds = 0.015f;
    float decaySeconds = 0.15f;
    float sustainLevel = 0.70f;
    float releaseSeconds = 0.16f;

    float outputTrim = 1.0f;             // per-character level match, calibrated
    float gestureSeconds = 0.25f;        // how long the onset contour takes
    float gestureDepth = 1.0f;           // scales the contour
    PitchContour pitchContour {};
};

// Index order is load-bearing: it matches both the JUCE AudioParameterChoice and
// the UI colour themes (0 red, 1 purple, 2 ice). Do not reorder.
inline constexpr std::array<CharacterPreset,
                            static_cast<std::size_t>(SealCharacter::count)> characterPresets {{
    // ---- 0: BURP (bass, red) ------------------------------------------------
    // Reference fundamental about 80 Hz. Signature is irregular period doubling
    // with roughly half the energy as noise (measured HNR +1.2 dB).
    {
        "burp", "BURP",
        // Gains follow the measured band split 36 / 50 / 11 percent, converted
        // from energy to amplitude: sqrt(50/36) = 1.18, sqrt(11/36) = 0.55.
        {{ {  380.0f, 120.0f, 1.45f, 0.85f },   // source, carries 300-800 Hz
           { 1082.0f,  68.0f, 1.18f, 0.55f },   // F1, Q 16
           { 2523.0f,  48.0f, 0.44f, 0.55f },   // F2, Q 53
           { 6231.0f,  93.0f, 0.08f, 0.30f },
           { 7995.0f, 461.0f, 0.035f, 0.00f } }},
        0.0022f,   // skirt
        0.64f,     // noise: HNR +1.2 dB
        0.035f,    // jitter, high: this is the growl
        0.30f,     // shimmer
        0.060f,    // period doubling, about one flip every 16 periods
        0.25f,     // sub octave for bass body
        0.015f, 0.25f, 0.72f, 0.18f,
        1.00f,
        0.35f, 1.0f,
        {{ +0.6f, +0.2f, 0.0f, -0.1f, -0.15f, -0.2f, -0.2f, -0.2f }}
    },
    // ---- 1: SQUEAL (default, purple) ---------------------------------------
    // Short bright bursts, 0.14 to 0.24 s. The contour is measured: a V-shaped
    // dip to -1.7 semitones then a rise to +1.9 before falling back.
    {
        "squeal", "SQUEAL",
        // Band split 62 / 36 / 1.8 percent: sqrt(36/62) = 0.76, sqrt(1.8/62) = 0.17.
        {{ {  555.0f, 110.0f, 0.74f, 1.00f },   // source
           { 1330.0f, 140.0f, 1.05f, 0.70f },   // F1
           { 3340.0f, 300.0f, 0.35f, 0.40f },   // F2
           { 6650.0f, 320.0f, 0.09f, 0.00f },
           { 8990.0f, 400.0f, 0.04f, 0.00f } }},
        0.0012f,   // tighter skirt: brighter
        0.21f,     // noise: HNR +17.5 dB
        0.006f,
        0.08f,
        0.0f,
        0.0f,
        0.015f, 0.12f, 0.55f, 0.14f,
        1.00f,
        0.20f, 1.0f,
        {{ +1.3f, -0.4f, -1.3f, -1.7f, -0.7f, +0.6f, +1.9f, 0.0f }}
    },
    // ---- 2: GROAN (high, ice) ----------------------------------------------
    // Sustained and very pure (HNR +25 to +31 dB) with a slow 280 ms swell. The
    // narrow first formant is the entire identity of this sound.
    {
        "groan", "GROAN",
        // Band split 31 / 67 percent: sqrt(67/31) = 1.47. The narrow F1 is the
        // whole identity of this sound.
        {{ {  620.0f,  55.0f, 0.70f, 1.00f },   // source
           { 1120.0f,  34.0f, 1.90f, 0.90f },   // singing formant, Q about 33
           { 2700.0f, 300.0f, 0.30f, 0.40f },
           { 6000.0f, 400.0f, 0.04f, 0.00f },
           { 8600.0f, 500.0f, 0.02f, 0.00f } }},
        0.0030f,   // wide skirt: soft and rounded
        0.10f,     // noise: HNR +28 dB
        0.003f,
        0.04f,
        0.0f,
        0.0f,
        0.280f, 0.40f, 0.85f, 0.35f,
        1.00f,
        0.50f, 1.0f,
        {{ -1.2f, -0.6f, -0.2f, 0.0f, +0.1f, +0.15f, +0.2f, +0.2f }}
    }
}};

inline constexpr const CharacterPreset& characterPreset(SealCharacter character) noexcept
{
    return characterPresets[static_cast<std::size_t>(
        isKnownSealCharacter(character) ? character : defaultSealCharacter)];
}

// Linear interpolation through the contour table for a normalised gesture
// position in [0, 1]. Returns semitones.
inline constexpr float pitchContourAt(const PitchContour& contour, float position) noexcept
{
    const auto clamped = position < 0.0f ? 0.0f : (position > 1.0f ? 1.0f : position);
    const auto scaled = clamped * static_cast<float>(pitchContourPoints - 1);
    const auto lower = static_cast<std::size_t>(scaled);
    const auto upper = lower + 1 < pitchContourPoints ? lower + 1 : lower;
    const auto fraction = scaled - static_cast<float>(lower);
    return contour[lower] + fraction * (contour[upper] - contour[lower]);
}
}
