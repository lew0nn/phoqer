#pragma once

#include <JuceHeader.h>

// Experimental "Pure 98" editor (PHOQER Test UI target only).
// Shared palette, fonts and Win98 drawing helpers.
namespace phoqer::testui
{
// Per-character palette: synthwave colours on classic Win98 material.
struct Palette
{
    juce::Colour accent;      // title bars, selections, LEDs, traces
    juce::Colour secondary;   // highlight partner of the accent
    juce::Colour sky;         // desktop and darkest bitmap tone
};

const Palette& paletteFor(int character) noexcept;     // 0 burp, 1 squeal, 2 groan

namespace win98
{
inline const juce::Colour face { 0xffc0c0c0 }, light { 0xffffffff }, shadow { 0xff808080 }, dark { 0xff0a0a0a };
}

struct Fonts
{
    juce::Typeface::Ptr pixel, pixelBold, logo, roboto;
    static const Fonts& get();
};

juce::Font pixelFont(float height, bool bold = false);
juce::Font logoFont(float height);

void bevel(juce::Graphics&, juce::Rectangle<float>, bool raised);
void button98(juce::Graphics&, juce::Rectangle<float>, bool pressed);
void sunken(juce::Graphics&, juce::Rectangle<float>, juce::Colour fill = juce::Colours::white);

// Draws a Win98 window (frame and title bar) and returns its client area. Caption buttons are real
// components (TitleButton98), placed by the editor only where they do something.
inline constexpr float defaultTitleHeight = 16.0f;
juce::Rectangle<float> windowTitleBar(juce::Rectangle<float> bounds, float titleHeight = defaultTitleHeight) noexcept;
juce::Rectangle<float> window98(juce::Graphics&, juce::Rectangle<float> bounds, const juce::String& title,
                                juce::Colour titleColour, bool active, float titleHeight = defaultTitleHeight);
juce::Rectangle<float> windowClient(juce::Rectangle<float> bounds, float titleHeight = defaultTitleHeight) noexcept;

void statusBar(juce::Graphics&, juce::Rectangle<float>, const juce::StringArray& fields);

void drawText(juce::Graphics&, const juce::String&, juce::Rectangle<float>, const juce::Font&, juce::Colour,
              juce::Justification = juce::Justification::centredLeft);

// Glyphs for the five behaviour modes (CALL, HONK, BARK, WAIL, MURMUR).
juce::Path modeGlyph(int mode, juce::Rectangle<float> area);

float hash(int x, int y) noexcept;
}
