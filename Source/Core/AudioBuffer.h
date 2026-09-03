#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <vector>

namespace phoqer
{
class AudioBuffer
{
public:
    static constexpr int maximumChannels = 8;

    AudioBuffer(float** channels, int channelCount, int sampleCount) noexcept
        : numChannels(std::clamp(channelCount, 0, maximumChannels)), numSamples(std::max(0, sampleCount))
    {
        for (int channel = 0; channel < numChannels; ++channel)
            channelData[static_cast<size_t>(channel)] = channels[channel];
    }

    AudioBuffer(int channelCount, int sampleCount)
        : numChannels(std::clamp(channelCount, 0, maximumChannels)), numSamples(std::max(0, sampleCount)),
          ownedData(static_cast<size_t>(numChannels * numSamples), 0.0f)
    {
        for (int channel = 0; channel < numChannels; ++channel)
            channelData[static_cast<size_t>(channel)] = ownedData.data() + channel * numSamples;
    }

    int getNumChannels() const noexcept { return numChannels; }
    int getNumSamples() const noexcept { return numSamples; }

    float* getWritePointer(int channel) noexcept
    {
        assert(channel >= 0 && channel < numChannels);
        return channelData[static_cast<size_t>(channel)];
    }

    const float* getReadPointer(int channel) const noexcept
    {
        assert(channel >= 0 && channel < numChannels);
        return channelData[static_cast<size_t>(channel)];
    }

    float getSample(int channel, int sample) const noexcept { return getReadPointer(channel)[sample]; }
    void setSample(int channel, int sample, float value) noexcept { getWritePointer(channel)[sample] = value; }
    void addSample(int channel, int sample, float value) noexcept { getWritePointer(channel)[sample] += value; }

    void clear() noexcept
    {
        for (int channel = 0; channel < numChannels; ++channel)
            std::fill_n(getWritePointer(channel), numSamples, 0.0f);
    }

    float getMagnitude(int startSample, int sampleCount) const noexcept
    {
        const auto first = std::clamp(startSample, 0, numSamples);
        const auto last = std::clamp(first + sampleCount, first, numSamples);
        float magnitude = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = first; sample < last; ++sample)
                magnitude = std::max(magnitude, std::abs(getSample(channel, sample)));
        return magnitude;
    }

private:
    int numChannels = 0;
    int numSamples = 0;
    std::array<float*, maximumChannels> channelData {};
    std::vector<float> ownedData;
};
}
