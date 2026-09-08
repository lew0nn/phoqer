#pragma once

#include <JuceHeader.h>

#include <array>

namespace phoqer::ui
{
enum class VisualStyle : int
{
    purple = 0,
    ice,
    red
};

VisualStyle visualStyleForCharacter(int characterIndex) noexcept;

struct ThemeAssets final
{
    juce::Image seal;
    juce::Image knobBody;
    juce::Image tideBody, tideTrack, tideFill, tideHandle;
    juce::Image meterOff, meterOn, waveFrame, led, menu;
    std::array<juce::Image, 5> modeButtons;

    juce::Colour accent;
    juce::Colour accentBright;
    juce::Colour accentSoft;
    juce::Colour text;
    juce::Colour textMuted;
    juce::Colour display;
    juce::Colour shellTop;
    juce::Colour shellBottom;
    juce::Colour panelTop;
    juce::Colour panelBottom;
    juce::Colour metalLight;
    juce::Colour metalDark;
    juce::Colour hairline;
};

class ThemeStore final
{
public:
    ThemeStore();

    const ThemeAssets& get(VisualStyle id) const noexcept;
    const ThemeAssets& get(int id) const noexcept;

private:
    std::array<ThemeAssets, 3> themes;
};
}
