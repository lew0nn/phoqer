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

inline constexpr size_t articulationVowels=9;
using ArticulationMap=std::array<std::array<FormantSpec,formantBandCount>,articulationVowels>;

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
    float wobbleSemitones = 0.0f;        // depth of the random pitch drift
    float wobbleRateHz = 12.0f;          // how often the drift picks a new target
    float flutterSemitones = 0.0f;       // a fast, slightly irregular vibrato (GROAN)
    float flutterRateHz = 12.5f;
    float squeakNoise = 0.0f;            // noise above 2.5 kHz that follows the call (SQUEAL)
    float onsetPunch = 1.0f;             // the bark spike at the start of a call, 0 to 1
    float thump = 0.0f;                  // a kick-like low hit under the attack, 0 to 1
    float vowelOffset = 0.0f;            // the seal's own vowel colour, added to VOWEL
    // An optional octave bass layer under the grains (KYRAX). Off at zero, as on the built-in seals.
    // General engine features any voice file can use; no code here belongs to a paid voice.
    float bassGain = 0.0f;
    float bassDrive = 0.0f;
    float bassDecaySeconds = 0.10f;
    float bassSustain = 0.45f;
    float spectralBodyGain = 1.0f;       // the grains' level when the bass layer is on
    // An optional nine-vowel map: VOWEL then moves the formants across the whole map (KYRAX).
    bool fullVowelMap = false;
    ArticulationMap articulationMap {};
};

// The three built-in voices, each value set by name (a value can never land in the wrong field).
// Built at compile time: no cost when the plugin runs.

// ---- BURP (bass, red) ------------------------------------------------------
// Reference fundamental about 82 Hz, calls about half a second long. Energy
// sits flat from 600 Hz to 2.5 kHz and falls away above 3 kHz, with a small
// lift near 6.4 kHz. Almost all voiced (HNR about +6 dB, spectral flatness
// 0.013): the roughness is jitter and period doubling, not breath.
inline constexpr CharacterPreset makeBurp() noexcept
{
    CharacterPreset p;
    p.id = "burp";
    p.displayName = "BURP";
    p.formants = {{ {  640.0f, 180.0f, 1.00f, 0.50f },   // source
                    { 1050.0f, 160.0f, 1.00f, 0.30f },   // F1
                    { 1750.0f, 350.0f, 2.70f, 0.00f },   // F2
                    { 2450.0f, 300.0f, 1.60f, 0.00f },   // F3, then the drop above 3 kHz
                    { 6400.0f, 500.0f, 0.20f, 0.00f } }};
    p.attackSkirtSeconds = 0.0030f;
    p.noiseMix = 0.06f;              // a trace only
    p.jitter = 0.010f;               // half the clip's, so it growls but stays a pitched note
    p.shimmer = 0.06f;               // little period-to-period flicker, a clean tone
    p.subharmonicChance = 0.040f;    // period doubling: the growl, a little rarer
    p.subOctaveGain = 0.04f;         // a little sub for body
    p.attackSeconds = 0.004f;        // tight: a hit, then hold steady
    p.decaySeconds = 0.09f;
    p.sustainLevel = 0.72f;
    p.releaseSeconds = 0.12f;
    p.outputTrim = 2.75f;            // back to its old loudness now the hiss is gone
    p.gestureSeconds = 0.45f;
    p.gestureDepth = 1.35f;          // so the default TIDE gives the measured contour
    p.pitchContour = {{ +0.3f, 0.0f, +0.5f, +0.2f, 0.0f, -0.4f, -0.2f, 0.0f }};   // ends on the note
    p.wobbleSemitones = 0.12f;       // a little life, not a pitch drift
    p.wobbleRateHz = 10.0f;
    p.flutterSemitones = 0.0f;
    p.flutterRateHz = 12.5f;
    p.squeakNoise = 0.0f;
    p.onsetPunch = 0.45f;            // a firm bark
    p.thump = 0.08f;                 // and only a trace of kick: more read as a thump
    p.vowelOffset = 0.0f;            // its own rough "ah"
    return p;
}

// ---- SQUEAL (purple) ---------------------------------------------------------
// Short yelps, median 0.18 s, at about 530 Hz. Each starts high, about +7
// semitones, drops, then lifts again; the pitch never holds (fast wobble
// 1.5 semitones). Harmonics fall steadily, about 4 dB each, above 500 Hz.
inline constexpr CharacterPreset makeSqueal() noexcept
{
    CharacterPreset p;
    p.id = "squeal";
    p.displayName = "SQUEAL";
    p.formants = {{ {  560.0f, 150.0f, 0.90f, 1.00f },   // source, on the fundamental
                    { 1050.0f, 250.0f, 1.50f, 0.60f },   // F1
                    { 1650.0f, 450.0f, 2.50f, 0.50f },   // F2
                    { 2700.0f, 250.0f, 0.90f, 0.00f },   // a nasal ring
                    { 6000.0f, 600.0f, 0.15f, 0.00f } }};
    p.attackSkirtSeconds = 0.0025f;
    p.noiseMix = 0.04f;              // a trace only
    p.jitter = 0.004f;               // next to none, so the yelp sits on the played note
    p.shimmer = 0.03f;
    p.subharmonicChance = 0.0f;      // no period doubling
    p.subOctaveGain = 0.0f;
    p.attackSeconds = 0.006f;        // tight yelps, a short release so they stay apart
    p.decaySeconds = 0.06f;
    p.sustainLevel = 0.65f;
    p.releaseSeconds = 0.05f;
    p.outputTrim = 1.48f;            // back to its old loudness
    p.gestureSeconds = 0.14f;
    p.gestureDepth = 1.35f;
    p.pitchContour = {{ +3.0f, 0.0f, -0.7f, -0.7f, -0.3f, 0.0f, 0.0f, 0.0f }};   // the yelp's dip, landing on the note
    p.wobbleSemitones = 0.0f;        // no random drift: an exact pitch
    p.wobbleRateHz = 14.0f;
    p.flutterSemitones = 0.0f;
    p.flutterRateHz = 12.5f;
    p.squeakNoise = 0.020f;          // the noisy squeak above the harmonics in each yelp
    p.onsetPunch = 0.60f;
    p.thump = 0.25f;
    p.vowelOffset = 0.22f;           // brighter, toward "eh"
    return p;
}

// ---- GROAN (high, ice) -------------------------------------------------------
// Long calls, about 1.8 s, near 620 Hz, scooping up 3.5 semitones into the
// note. Voiced dark and mellow (chosen by ear, 2026-10-06): the measured
// singing formant at 1120 Hz with a 34 Hz bandwidth was so narrow it played
// as a pure whistle and sounded cheap, so it is lower and wider here, the
// top is softer, the onset rounder, and a slow vibrato replaces the clip's
// fast 12.5 Hz flutter. A warm "ooh" moan.
inline constexpr CharacterPreset makeGroan() noexcept
{
    CharacterPreset p;
    p.id = "groan";
    p.displayName = "GROAN";
    p.formants = {{ {  620.0f, 100.0f, 1.60f, 1.00f },   // source, on the fundamental
                    { 1050.0f,  95.0f, 1.40f, 0.90f },   // singing formant: lower and wider, a moan not a whistle
                    { 1700.0f, 400.0f, 2.50f, 0.00f },
                    { 2800.0f, 400.0f, 0.60f, 0.00f },
                    { 6400.0f, 600.0f, 0.20f, 0.00f } }};
    p.attackSkirtSeconds = 0.0050f;  // a wider skirt still: a soft, rounded onset
    p.noiseMix = 0.008f;             // a trace only, the clip is clean between harmonics
    p.jitter = 0.002f;
    p.shimmer = 0.02f;
    p.subharmonicChance = 0.0f;
    p.subOctaveGain = 0.0f;
    p.attackSeconds = 0.620f;        // a slow swell to a peak near 460 ms
    p.decaySeconds = 0.40f;
    p.sustainLevel = 0.55f;
    p.releaseSeconds = 0.35f;
    p.outputTrim = 1.00f;
    p.gestureSeconds = 1.60f;
    p.gestureDepth = 1.35f;
    p.pitchContour = {{ -3.5f, -0.6f, 0.0f, 0.0f, 0.0f, +0.3f, +0.1f, 0.0f }};   // the scoop up, then on the note
    p.wobbleSemitones = 0.06f;
    p.wobbleRateHz = 8.0f;
    p.flutterSemitones = 0.22f;      // a gentle slow vibrato (the clip's 12.5 Hz flutter read as a toy whistle)
    p.flutterRateHz = 5.0f;
    p.squeakNoise = 0.0f;
    p.onsetPunch = 0.15f;
    p.thump = 0.0f;
    p.vowelOffset = 0.0f;            // its own, as measured (a darker colour turned it into a whistle)
    return p;
}

// Index order is load-bearing: it matches both the JUCE AudioParameterChoice and
// the UI colour themes (0 red, 1 purple, 2 ice). Do not reorder.
inline constexpr std::array<CharacterPreset, static_cast<std::size_t>(SealCharacter::count)> characterPresets {{
    makeBurp(), makeSqueal(), makeGroan() }};

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
