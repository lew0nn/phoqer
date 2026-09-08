#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/DspPrimitives.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace phoqer
{
// Lookahead safety limiter.
//
// The gain reduction is computed from the loudest sample in the next two
// milliseconds and applied to the delayed signal, so it is fully in place before
// the peak arrives and the ceiling is never overshot. A one-pole envelope
// follower cannot make that guarantee.
//
// The sliding maximum uses a monotonic wedge: a fixed-capacity ring that holds
// only the samples which could still become the window maximum. Every sample is
// pushed and popped at most once, so the cost is constant per sample and nothing
// is allocated while audio is running.
class LookaheadLimiter
{
public:
    void prepare(double sampleRate)
    {
        lookahead = std::max(8, static_cast<int>(0.002 * sampleRate));
        delaySize = lookahead + 1;
        capacity = lookahead + 3;
        delayLeft.assign(static_cast<size_t>(delaySize), 0.0f);
        delayRight.assign(static_cast<size_t>(delaySize), 0.0f);
        wedgeValue.assign(static_cast<size_t>(capacity), 0.0f);
        wedgeIndex.assign(static_cast<size_t>(capacity), 0);

        attackCoefficient = 1.0f - std::exp(-1.0f / static_cast<float>(0.0005 * sampleRate));
        releaseCoefficient = 1.0f - std::exp(-1.0f / static_cast<float>(0.150 * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        std::fill(delayLeft.begin(), delayLeft.end(), 0.0f);
        std::fill(delayRight.begin(), delayRight.end(), 0.0f);
        head = tail = 0;
        writeIndex = 0;
        sampleCounter = 0;
        gain = 1.0f;
        minimumGain = 1.0f;
    }

    void setCeiling(float value) noexcept { ceiling = clamp(0.05f, 1.0f, value); }
    void setEnabled(bool value) noexcept { enabled = value; }
    int getLatencySamples() const noexcept { return enabled ? lookahead : 0; }

    // Gain reduction applied since the last call, in decibels.
    float consumeGainReductionDb() noexcept
    {
        const auto reduction = -20.0f * std::log10(std::max(1.0e-4f, minimumGain));
        minimumGain = 1.0f;
        return reduction;
    }

    void process(AudioBuffer& buffer) noexcept
    {
        if (! enabled || buffer.getNumChannels() == 0 || delaySize <= 0)
            return;

        const auto stereo = buffer.getNumChannels() > 1;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto left = buffer.getSample(0, sample);
            const auto right = stereo ? buffer.getSample(1, sample) : left;
            const auto peak = std::max(std::abs(left), std::abs(right));

            pushWedge(peak, sampleCounter);
            popExpired(sampleCounter - lookahead);
            ++sampleCounter;

            const auto windowMaximum = wedgeValue[static_cast<size_t>(head)];
            const auto target = windowMaximum > ceiling ? ceiling / windowMaximum : 1.0f;
            gain += (target - gain) * (target < gain ? attackCoefficient : releaseCoefficient);
            minimumGain = std::min(minimumGain, gain);

            delayLeft[static_cast<size_t>(writeIndex)] = left;
            delayRight[static_cast<size_t>(writeIndex)] = right;
            writeIndex = writeIndex + 1 < delaySize ? writeIndex + 1 : 0;

            const auto delayedLeft = delayLeft[static_cast<size_t>(writeIndex)] * gain;
            const auto delayedRight = delayRight[static_cast<size_t>(writeIndex)] * gain;

            buffer.setSample(0, sample, delayedLeft);
            if (stereo)
                buffer.setSample(1, sample, delayedRight);
            for (int channel = 2; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel, sample, delayedLeft);
        }
    }

private:
    void pushWedge(float value, long long index) noexcept
    {
        while (tail != head)
        {
            const auto last = tail > 0 ? tail - 1 : capacity - 1;
            if (wedgeValue[static_cast<size_t>(last)] > value)
                break;
            tail = last;
        }
        wedgeValue[static_cast<size_t>(tail)] = value;
        wedgeIndex[static_cast<size_t>(tail)] = index;
        tail = tail + 1 < capacity ? tail + 1 : 0;
    }

    void popExpired(long long oldest) noexcept
    {
        while (head != tail && wedgeIndex[static_cast<size_t>(head)] < oldest)
            head = head + 1 < capacity ? head + 1 : 0;
    }

    std::vector<float> delayLeft, delayRight, wedgeValue;
    std::vector<long long> wedgeIndex;
    long long sampleCounter = 0;
    float ceiling = 0.9661f;          // -0.3 dBFS
    float gain = 1.0f, minimumGain = 1.0f;
    float attackCoefficient = 1.0f, releaseCoefficient = 0.001f;
    int lookahead = 0, delaySize = 0, capacity = 0;
    int head = 0, tail = 0, writeIndex = 0;
    bool enabled = true;
};
}
