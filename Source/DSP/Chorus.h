#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/DspPrimitives.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace phoqer
{
// DETUNE: a unison width. Each side gets a copy of the voice through a short
// delay whose length drifts slowly, so it sings a few cents off the dry voice;
// the two sides drift apart, which spreads the seal across the stereo field.
// At 0 it is bypassed entirely.
class Chorus
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        const auto size = static_cast<size_t>(std::ceil(0.05 * sampleRate)) + 4;
        for (auto& line : lines)
            line.assign(size, 0.0f);
        amount.reset(sampleRate, 0.05);
        amount.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset() noexcept
    {
        for (auto& line : lines)
            std::fill(line.begin(), line.end(), 0.0f);
        writeIndex = 0;
        phase = 0.0;
    }

    void process(AudioBuffer& buffer, float detune) noexcept
    {
        const auto target = clamp(0.0f, 1.0f, detune);
        amount.setTargetValue(target);
        if (buffer.getNumChannels() < 2 || lines[0].empty())
            return;
        if (lastAmount <= 0.0f && target <= 0.0f)
        {
            if (! idle) { reset(); idle = true; }    // bypassed: start clean when it comes back
            return;
        }
        idle = false;

        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        const auto size = static_cast<int>(lines[0].size());
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto a = lastAmount = amount.getNextValue();
            lines[0][static_cast<size_t>(writeIndex)] = left[sample];
            lines[1][static_cast<size_t>(writeIndex)] = right[sample];

            phase += 0.55 / sampleRate;
            phase -= std::floor(phase);
            // two slow LFOs a quarter cycle apart, one per side; deeper with DETUNE
            const auto depth = 0.0006 + 0.0034 * a;        // seconds of sweep
            const double sweep[2] = { std::sin(twoPi<double> * phase),
                                      std::cos(twoPi<double> * phase * 1.13) };
            float wet[2];
            for (int side = 0; side < 2; ++side)
            {
                const auto delay = (0.012 + depth * (0.5 + 0.5 * sweep[side])) * sampleRate;
                auto read = static_cast<double>(writeIndex) - delay;
                while (read < 0.0)
                    read += size;
                const auto i0 = static_cast<int>(read);
                const auto i1 = (i0 + 1) % size;
                const auto fraction = static_cast<float>(read - i0);
                const auto& line = lines[static_cast<size_t>(side)];
                wet[side] = line[static_cast<size_t>(i0)] + fraction * (line[static_cast<size_t>(i1)] - line[static_cast<size_t>(i0)]);
            }
            if (++writeIndex >= size)
                writeIndex = 0;

            // The copies cross over (left's copy to the right) for width; the dry
            // level dips a little so the sum stays about as loud.
            const auto wetGain = 0.55f * a, dryGain = 1.0f - 0.25f * a;
            const auto l = left[sample], r = right[sample];
            left[sample] = l * dryGain + wet[1] * wetGain;
            right[sample] = r * dryGain + wet[0] * wetGain;
        }
    }

private:
    double sampleRate = 44100.0, phase = 0.0;
    std::vector<float> lines[2];
    int writeIndex = 0;
    float lastAmount = 0.0f;
    bool idle = true;
    LinearSmoother amount;
};
}
