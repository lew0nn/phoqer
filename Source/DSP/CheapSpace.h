#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/DspPrimitives.h"
#include <array>
#include <vector>

namespace phoqer
{
class CheapSpace
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void process(AudioBuffer& buffer, float spaceAmount) noexcept;

private:
    static constexpr int lineCount = 4;
    struct DelayLine
    {
        std::vector<float> data;
        int index = 0;
    };
    static constexpr int diffuserCount = 2;
    std::array<DelayLine, lineCount> lines;
    std::array<DelayLine, diffuserCount> diffusers;
    std::array<float, lineCount> damp {};
    LinearSmoother wet;
};
}
