#pragma once

#include <JuceHeader.h>

// PHOQER's "Pure 98" editor.
// Shared palette, fonts and Win98 drawing helpers.
namespace phoqer::ui
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
// The title bar fades between the voice's two colours, as Win98's did: bright on the main window (active),
// deeper and dimmer on the windows inside it.
juce::Rectangle<float> window98(juce::Graphics&, juce::Rectangle<float> bounds, const juce::String& title,
                                const Palette& palette, bool active, float titleHeight = defaultTitleHeight);
juce::Rectangle<float> windowClient(juce::Rectangle<float> bounds, float titleHeight = defaultTitleHeight) noexcept;

void statusBar(juce::Graphics&, juce::Rectangle<float>, const juce::StringArray& fields);

void drawText(juce::Graphics&, const juce::String&, juce::Rectangle<float>, const juce::Font&, juce::Colour,
              juce::Justification = juce::Justification::centredLeft);

// Win98 look for the parts JUCE draws itself: popup menus, tooltips and the window's size grip.
// Menu items whose IDs fall in the radio range show a dot when ticked (one-of-several); any other
// ticked item shows a checkmark (on/off).
class Win98LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static constexpr int radioIdFirst = 2000, radioIdLast = 2999;

    Win98LookAndFeel();
    void setCharacter(int c) noexcept { character = c; }

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackgroundWithOptions(juce::Graphics&, int width, int height, const juce::PopupMenu::Options&) override;
    void drawPopupMenuItemWithOptions(juce::Graphics&, const juce::Rectangle<int>& area, bool isHighlighted,
                                      const juce::PopupMenu::Item&, const juce::PopupMenu::Options&) override;
    void getIdealPopupMenuItemSizeWithOptions(const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                              int& idealWidth, int& idealHeight, const juce::PopupMenu::Options&) override;
    int getPopupMenuBorderSizeWithOptions(const juce::PopupMenu::Options&) override { return 4; }

    juce::Rectangle<int> getTooltipBounds(const juce::String& text, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;

    void drawCornerResizer(juce::Graphics&, int width, int height, bool isMouseOver, bool isMouseDragging) override;

private:
    int character = 0;
};

// Glyphs for the five behaviour modes (CALL, HONK, BARK, WAIL, MURMUR).
juce::Path modeGlyph(int mode, juce::Rectangle<float> area);

float hash(int x, int y) noexcept;
}
