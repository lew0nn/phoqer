#pragma once

#include <JuceHeader.h>

#include <array>
#include <functional>

// YAP, ARP and NITRO, KYRAX's three switches, and the parts of NITRO.EXE and ARP.EXE.
namespace phoqer::ui
{
// The colour of each NITRO note: on its key, its chip and its level slider. The key's own note is yellow.
juce::Colour nitroNoteColour(int slot) noexcept;
const juce::Colour nitroRootColour { 0xffffd228 };

// A small switch on dark glass, like the metronome's: lips (YAP), rising notes (ARP) or a bolt (NITRO). Pressed in and pink
// when on; raised and dim when off; grey when the voice is not KYRAX. A right-click calls onRightClick
// instead of switching.
class IconToggle98 final : public juce::Button
{
public:
    enum class Icon { lips, notes, bolt };
    explicit IconToggle98(Icon);
    std::function<void()> onRightClick;
    void paintButton(juce::Graphics&, bool over, bool down) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    Icon icon;
};

// NITRO.EXE's keyboard, two octaves either side of the key (-12 to +24). Tap a key to add that note,
// tap it again to take it away; three notes at most. The key itself is yellow, each note in its colour.
class NitroKeys98 final : public juce::Component
{
public:
    NitroKeys98(std::array<juce::RangedAudioParameter*, 3> notes);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    static constexpr int lowest = -12, highest = 24;
    int keyX(int offset) const;                       // the left edge of a white key, for the labels under it
    float whiteWidth() const;

private:
    int offsetAt(juce::Point<float>) const;
    std::array<juce::RangedAudioParameter*, 3> notes;
};

// A Win98 push button that stays in when chosen: ARP.EXE's RATE and HOLD rows. Clicking calls onPick.
class SegmentButton98 final : public juce::Button
{
public:
    explicit SegmentButton98(const juce::String& text) : juce::Button(text) {}
    std::function<void()> onPick;
    void paintButton(juce::Graphics&, bool over, bool down) override;
    void clicked() override { if (onPick) onPick(); }
};

// ARP.EXE's step preview: what a C-E-G chord plays with the chosen PATTERN and OCTAVES, one lit cell per step
// on a dark grid, low notes at the bottom. RANDOM shows a fixed example.
class ArpSteps98 final : public juce::Component
{
public:
    std::function<int()> pattern, octaves;
    void paint(juce::Graphics&) override;
    static constexpr int steps = 12;
};

// A level slider in its note's colour: a dark track that fills up to the value, and a Win98 thumb.
class LevelSlider98 final : public juce::Slider
{
public:
    LevelSlider98() : juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox) {}
    juce::Colour colour;
    void paint(juce::Graphics&) override;
};
}
