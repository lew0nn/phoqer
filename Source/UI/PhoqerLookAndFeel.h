#pragma once

#include <JuceHeader.h>

#include "PhoqerTheme.h"

namespace phoqer::ui
{
class PhoqerLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PhoqerLookAndFeel();

    void setTheme(const ThemeAssets& newTheme) noexcept;

    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawPopupMenuBackground(juce::Graphics&, int, int) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>&, bool, bool,
                           bool, bool, bool, const juce::String&, const juce::String&,
                           const juce::Drawable*, const juce::Colour*) override;

private:
    const ThemeAssets* theme = nullptr;
};
}
