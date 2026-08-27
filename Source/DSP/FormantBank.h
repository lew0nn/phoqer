#pragma once

#include "../Core/PhoqerTypes.h"
#include "../Voice/VoicePersonality.h"
#include <array>

namespace phoqer
{
class FormantBank
{
public:
    struct VowelDescription
    {
        std::array<float, 4> frequency {};
        std::array<float, 4> q {};
        std::array<float, 4> gain {};
    };

    void prepare(double sampleRate);
    void reset() noexcept;
    static VowelDescription describeVowel(float morph) noexcept;
    float process(float input, float midiNote, const MacroState&, const VocalState&,
                  const VoicePersonality&, float tideMovement) noexcept;

private:
    struct Resonator
    {
        void reset() noexcept { ic1eq = ic2eq = 0.0f; }
        float process(float input, float frequency, float q, float sampleRate) noexcept;
        float ic1eq = 0.0f, ic2eq = 0.0f;
        float smoothedFrequency = 1000.0f;
        float smoothedQ = 6.0f;
        float smoothedGain = 0.25f;
    };

    std::array<Resonator, 4> resonators;
    double currentSampleRate = 44100.0;
};
}
