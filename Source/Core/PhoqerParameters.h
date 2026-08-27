#pragma once

#include <JuceHeader.h>

namespace phoqer::parameters
{
inline constexpr auto boom = "boom";
inline constexpr auto air = "air";
inline constexpr auto bark = "bark";
inline constexpr auto vowel = "vowel";
inline constexpr auto space = "space";
inline constexpr auto tide = "tide";
inline constexpr auto output = "output";

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto normal = juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f };
    layout.add(std::make_unique<juce::AudioParameterFloat>(boom, "BOOM", normal, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(air, "AIR", normal, 0.25f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(bark, "BARK", normal, 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(vowel, "VOWEL", normal, 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(space, "SPACE", normal, 0.2f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(tide, "TIDE", normal, 0.25f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        output, "OUTPUT", juce::NormalisableRange<float> { -24.0f, 6.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    return layout;
}
}
