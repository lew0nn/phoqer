#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

namespace phoqer
{
class CheapSpace
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void process(juce::AudioBuffer<float>& buffer, float spaceAmount) noexcept;

private:
    static constexpr int lineCount = 4;
    struct DelayLine
    {
        std::vector<float> data;
        int index = 0;
    };
    std::array<DelayLine, lineCount> lines;
    float dampLeft = 0.0f, dampRight = 0.0f;
    juce::SmoothedValue<float> wet;
};
}

