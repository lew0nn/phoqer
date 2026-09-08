#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/DspPrimitives.h"
#include "Limiter.h"

#include <algorithm>
#include <cmath>

namespace phoqer
{
class OutputStage
{
public:
    void prepare(double sampleRate)
    {
        outputGain.reset(sampleRate, 0.04);
        outputGain.setCurrentAndTargetValue(1.0f);
        limiter.prepare(sampleRate);
    }

    void reset() noexcept
    {
        meterRms = meterPeak = meterPreLimiterPeak = meterGainReductionDb = 0.0f;
        limiter.reset();
    }

    void setOutputDb(float decibels) noexcept { outputGain.setTargetValue(decibelsToGain(decibels)); }

    // The harness measures raw headroom with the limiter out of the way.
    void setLimiterEnabled(bool value) noexcept { limiter.setEnabled(value); }

    void process(AudioBuffer& buffer) noexcept
    {
        // User gain first, so the limiter protects against extreme settings too.
        float preLimiterPeak = 0.0f;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto gain = outputGain.getNextValue();
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto value = buffer.getSample(channel, sample) * gain;
                if (! std::isfinite(value))
                    value = 0.0f;
                buffer.setSample(channel, sample, value);
                preLimiterPeak = std::max(preLimiterPeak, std::abs(value));
            }
        }

        limiter.process(buffer);

        double sumSquares = 0.0;
        float blockPeak = 0.0f;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                // Belt and braces. With the limiter engaged this never bites.
                auto value = clamp(-1.0f, 1.0f, buffer.getSample(channel, sample));
                if (! std::isfinite(value))
                    value = 0.0f;
                buffer.setSample(channel, sample, value);
                blockPeak = std::max(blockPeak, std::abs(value));
                sumSquares += static_cast<double>(value) * value;
            }
        }

        const auto count = std::max(1, buffer.getNumSamples() * buffer.getNumChannels());
        const auto blockRms = static_cast<float>(std::sqrt(sumSquares / count));
        meterPeak = std::max(blockPeak, meterPeak * 0.88f);
        meterRms += 0.16f * (blockRms - meterRms);
        meterPreLimiterPeak = std::max(preLimiterPeak, meterPreLimiterPeak * 0.88f);
        meterGainReductionDb = std::max(limiter.consumeGainReductionDb(),
                                        meterGainReductionDb * 0.80f);
    }

    float getPeak() const noexcept { return meterPeak; }
    float getRms() const noexcept { return meterRms; }
    float getPreLimiterPeak() const noexcept { return meterPreLimiterPeak; }
    float getGainReductionDb() const noexcept { return meterGainReductionDb; }
    int getLatencySamples() const noexcept { return limiter.getLatencySamples(); }

private:
    LinearSmoother outputGain;
    LookaheadLimiter limiter;
    float meterPeak = 0.0f, meterRms = 0.0f;
    float meterPreLimiterPeak = 0.0f, meterGainReductionDb = 0.0f;
};
}
