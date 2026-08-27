#include "CheapSpace.h"

namespace phoqer
{
void CheapSpace::prepare(double sampleRate, int)
{
    constexpr std::array<double, lineCount> delaySeconds { 0.0297, 0.0371, 0.0411, 0.0533 };
    for (int line = 0; line < lineCount; ++line)
    {
        const auto size = juce::jmax(8, static_cast<int>(std::ceil(delaySeconds[static_cast<size_t>(line)] * sampleRate)));
        lines[static_cast<size_t>(line)].data.assign(static_cast<size_t>(size), 0.0f);
        lines[static_cast<size_t>(line)].index = 0;
    }
    wet.reset(sampleRate, 0.05);
    wet.setCurrentAndTargetValue(0.0f);
    reset();
}

void CheapSpace::reset() noexcept
{
    for (auto& line : lines)
    {
        std::fill(line.data.begin(), line.data.end(), 0.0f);
        line.index = 0;
    }
    dampLeft = dampRight = 0.0f;
}

void CheapSpace::process(juce::AudioBuffer<float>& buffer, float spaceAmount) noexcept
{
    wet.setTargetValue(juce::jlimit(0.0f, 0.42f, spaceAmount * 0.42f));
    const auto feedback = 0.46f + 0.22f * juce::jlimit(0.0f, 1.0f, spaceAmount);
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto input = 0.5f * (left[sample] + right[sample]);
        std::array<float, lineCount> taps {};
        for (int line = 0; line < lineCount; ++line)
        {
            auto& delay = lines[static_cast<size_t>(line)];
            taps[static_cast<size_t>(line)] = delay.data[static_cast<size_t>(delay.index)];
        }

        const auto sum = 0.5f * (taps[0] + taps[1] + taps[2] + taps[3]);
        const std::array<float, lineCount> mixed {
            sum - taps[0], sum - taps[1], sum - taps[2], sum - taps[3]
        };
        for (int line = 0; line < lineCount; ++line)
        {
            auto& delay = lines[static_cast<size_t>(line)];
            delay.data[static_cast<size_t>(delay.index)] = input * 0.28f + mixed[static_cast<size_t>(line)] * feedback;
            if (++delay.index >= static_cast<int>(delay.data.size())) delay.index = 0;
        }

        dampLeft += 0.24f * ((taps[0] + taps[2]) * 0.5f - dampLeft);
        dampRight += 0.24f * ((taps[1] + taps[3]) * 0.5f - dampRight);
        const auto mix = wet.getNextValue();
        left[sample] = left[sample] * (1.0f - 0.18f * mix) + dampLeft * mix;
        right[sample] = right[sample] * (1.0f - 0.18f * mix) + dampRight * mix;
    }
}
}
