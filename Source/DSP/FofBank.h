#pragma once

#include "FofFormant.h"
#include "GlottalPulse.h"
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

    float process(double frequencyHz, const CharacterPreset& preset,
                  const MacroState& macros, const VocalState& vocal,
                  Random& random) noexcept;

private:
    static FormantSpec shapeFormant(const FormantSpec& base, size_t index,
                                    const MacroState& macros, const VocalState& vocal,
                                    double fundamentalHz) noexcept;

    std::array<FofFormant, formantBandCount> formants {};
    GlottalPulse pulse;

    double sampleRate = 44100.0;
    double fundamentalPhase = 0.0;
    float noiseLowPass = 0.0f;
    float noiseBody = 0.0f;
    float previousNoise = 0.0f;
};
}
