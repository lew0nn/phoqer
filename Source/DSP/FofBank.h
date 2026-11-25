#pragma once

#include "FofFormant.h"
#include "GlottalPulse.h"
#include "BassUndertone.h"
#include "../Core/CharacterPreset.h"
#include "../Core/PhoqerTypes.h"
#include "../Core/Random.h"

#include <array>

namespace phoqer
{
// Four FOF formants, the breath-noise path and the fundamental sine, driven by
// one pitch-synchronous glottal trigger.
//
// FOF grains only put energy near the formant centres, so a character whose
// lowest formant is at 1082 Hz has no fundamental of its own. The fundamental
// sine supplies that body, which is what makes BURP work as a bass voice at
// about 80 Hz and what BOOM controls for every character.
class FofBank
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // The played note, so locked bands pick their harmonic from it and not from
    // wherever the onset scoop happens to be.
    void setNoteFrequency(double hz) noexcept { noteFrequency = hz; }
    // The fundamental sine's pitch, set at each control point (it moves far slower than a sample).
    void setFundamental(double hz) noexcept;

    float process(double frequencyHz, const CharacterPreset& preset,
                  const MacroState& macros, const VocalState& vocal,
                  Random& random) noexcept;

private:
    FormantSpec shapeFormant(const FormantSpec& base, size_t index,
                             const MacroState& macros, const VocalState& vocal,
                             double fundamentalHz, const CharacterPreset& preset) noexcept;

    std::array<FofFormant, formantBandCount> formants {};
    GlottalPulse pulse;
    BassUndertone bass;
    // The harmonic each locked band sits on, chosen once per note (0: not yet).
    std::array<double, formantBandCount> lockedHarmonic {};
    double noteFrequency = 0.0;
    static constexpr double highTuningFromHz = 262.0, highTuningFullHz = 600.0;     // C4, about D5
    static constexpr float highTuningTrim = 0.45f;
    static constexpr float yapTiltOctaves = 4.0f;     // YAP's brightness swing up high: about +-12 dB at the pattern's ends
    static constexpr float yapUnlock = 0.8f;          // how far YAP lets the vowel bands off their harmonic up high

    double sampleRate = 44100.0, inverseSampleRate = 1.0 / 44100.0;
    float fundamentalCos = 1.0f, fundamentalSin = 0.0f;      // the fundamental, as a rotating unit vector
    float fundamentalStepCos = 1.0f, fundamentalStepSin = 0.0f;
    float noiseLowPass = 0.0f;
    float noiseBody = 0.0f;
    float previousNoise = 0.0f;
    float noiseHigh = 0.0f;          // low-passed white, subtracted for the squeak
    float highCoefficient = 0.0f;
    float coefficientAir = -1.0f;    // the AIR the two breath coefficients were worked out for
    float airCoefficient = 0.0f, bodyCoefficient = 0.0f;
};
}
