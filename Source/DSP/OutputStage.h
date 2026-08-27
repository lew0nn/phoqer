#pragma once

#include <JuceHeader.h>

namespace phoqer
{
class OutputStage
{
public:
    void prepare(double sampleRate)
    {
        outputGain.reset(sampleRate, 0.04);
        outputGain.setCurrentAndTargetValue(1.0f);
    }

    void reset() noexcept { meterRms = meterPeak = 0.0f; }
    void setOutputDb(float decibels) noexcept { outputGain.setTargetValue(juce::Decibels::decibelsToGain(decibels)); }

    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        double sumSquares = 0.0;
        float blockPeak = 0.0f;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto gain = outputGain.getNextValue();
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto value = buffer.getSample(channel, sample) * gain;
                if (! std::isfinite(value)) value = 0.0f;
                value = std::tanh(value * 0.92f) / 0.92f;
                buffer.setSample(channel, sample, value);
                blockPeak = juce::jmax(blockPeak, std::abs(value));
                sumSquares += static_cast<double>(value) * value;
            }
        }
        const auto count = juce::jmax(1, buffer.getNumSamples() * buffer.getNumChannels());
        const auto blockRms = static_cast<float>(std::sqrt(sumSquares / count));
        meterPeak = juce::jmax(blockPeak, meterPeak * 0.88f);
        meterRms += 0.16f * (blockRms - meterRms);
    }

    float getPeak() const noexcept { return meterPeak; }
    float getRms() const noexcept { return meterRms; }

private:
    juce::SmoothedValue<float> outputGain;
    float meterPeak = 0.0f, meterRms = 0.0f;
};
}
