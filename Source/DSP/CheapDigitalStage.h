#pragma once

#include <JuceHeader.h>
#include <array>

namespace phoqer
{
class CheapDigitalStage
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset() noexcept;
    void process(juce::AudioBuffer<float>& buffer) noexcept;

private:
    std::array<float, 2> lowPass {}, held {};
    float lowPassCoefficient = 0.5f, holdPhase = 0.0f, holdIncrement = 0.5f;
};
}

