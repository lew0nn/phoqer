#include "CheapSpace.h"

#include <algorithm>
#include <cmath>

namespace phoqer
{
// REVERB: a small feedback-delay network. Two all-pass stages smear the input
// first so the tail builds smoothly instead of as a row of echoes; four longer
// lines, mixed by a Householder matrix, carry it. REVERB sets everything at
// once: from a short room (about 0.9 s) at the bottom to a dark cave (about
// 4 s) at the top, the tail damped more as it grows.
void CheapSpace::prepare(double sampleRate, int)
{
    constexpr std::array<double, lineCount> delaySeconds { 0.0437, 0.0571, 0.0683, 0.0797 };
    for (int line = 0; line < lineCount; ++line)
    {
        const auto size = std::max(8, static_cast<int>(std::ceil(delaySeconds[static_cast<size_t>(line)] * sampleRate)));
        lines[static_cast<size_t>(line)].data.assign(static_cast<size_t>(size), 0.0f);
        lines[static_cast<size_t>(line)].index = 0;
    }
    constexpr std::array<double, diffuserCount> diffuserSeconds { 0.0043, 0.0016 };
    for (int stage = 0; stage < diffuserCount; ++stage)
    {
        const auto size = std::max(4, static_cast<int>(std::ceil(diffuserSeconds[static_cast<size_t>(stage)] * sampleRate)));
        diffusers[static_cast<size_t>(stage)].data.assign(static_cast<size_t>(size), 0.0f);
        diffusers[static_cast<size_t>(stage)].index = 0;
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
    for (auto& stage : diffusers)
    {
        std::fill(stage.data.begin(), stage.data.end(), 0.0f);
        stage.index = 0;
    }
    damp.fill(0.0f);
}

void CheapSpace::process(AudioBuffer& buffer, float spaceAmount) noexcept
{
    const auto amount = clamp(0.0f, 1.0f, spaceAmount);
    wet.setTargetValue(amount * 0.55f);
    const auto feedback = 0.62f + 0.28f * amount;          // about 0.9 s to 4 s
    const auto brightness = lerp(amount, 0.55f, 0.28f);     // in-loop low-pass: darker as it grows
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto input = 0.5f * (left[sample] + right[sample]);
        for (auto& stage : diffusers)                       // Schroeder all-pass, g = 0.6
        {
            auto& slot = stage.data[static_cast<size_t>(stage.index)];
            const auto delayed = slot;
            const auto v = input + 0.6f * delayed;
            slot = v;
            input = delayed - 0.6f * v;
            if (++stage.index >= static_cast<int>(stage.data.size())) stage.index = 0;
        }

        std::array<float, lineCount> taps {};
        for (int line = 0; line < lineCount; ++line)
        {
            auto& delay = lines[static_cast<size_t>(line)];
            auto& d = damp[static_cast<size_t>(line)];
            d += brightness * (delay.data[static_cast<size_t>(delay.index)] - d);
            taps[static_cast<size_t>(line)] = d;
        }

        const auto sum = 0.5f * (taps[0] + taps[1] + taps[2] + taps[3]);
        for (int line = 0; line < lineCount; ++line)
        {
            auto& delay = lines[static_cast<size_t>(line)];
            delay.data[static_cast<size_t>(delay.index)] = input * 0.34f
                + (sum - taps[static_cast<size_t>(line)]) * feedback;
            if (++delay.index >= static_cast<int>(delay.data.size())) delay.index = 0;
        }

        const auto mix = wet.getNextValue();
        const auto dryGain = 1.0f - 0.30f * mix;
        const auto wetGain = 1.25f * mix;
        left[sample] = left[sample] * dryGain + (taps[0] - taps[2] * 0.5f) * wetGain;
        right[sample] = right[sample] * dryGain + (taps[1] - taps[3] * 0.5f) * wetGain;
    }
}
}
