#include "CheapDigitalStage.h"

namespace phoqer
{
void CheapDigitalStage::prepare(double sampleRate, int)
{
    const auto bandwidth = juce::jlimit(9000.0, sampleRate * 0.42, 12500.0);
    lowPassCoefficient = static_cast<float>(1.0 - std::exp(-juce::MathConstants<double>::twoPi * bandwidth / sampleRate));
    holdIncrement = static_cast<float>(juce::jmin(1.0, 24000.0 / sampleRate));
    reset();
}

void CheapDigitalStage::reset() noexcept
{
    lowPass.fill(0.0f);
    held.fill(0.0f);
    holdPhase = 1.0f;
}

void CheapDigitalStage::process(juce::AudioBuffer<float>& buffer) noexcept
{
    constexpr float quantizationSteps = 8192.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        holdPhase += holdIncrement;
        const bool refresh = holdPhase >= 1.0f;
        if (refresh) holdPhase -= 1.0f;

        for (int channel = 0; channel < juce::jmin(2, buffer.getNumChannels()); ++channel)
        {
            const auto input = buffer.getSample(channel, sample);
            lowPass[static_cast<size_t>(channel)] += lowPassCoefficient
                * (input - lowPass[static_cast<size_t>(channel)]);
            if (refresh)
            {
                const auto value = lowPass[static_cast<size_t>(channel)];
                const auto quantized = std::round(value * quantizationSteps) / quantizationSteps;
                held[static_cast<size_t>(channel)] = 0.86f * value + 0.14f * quantized;
            }
            buffer.setSample(channel, sample, std::tanh(held[static_cast<size_t>(channel)] * 1.035f) / 1.035f);
        }
    }
}
}
