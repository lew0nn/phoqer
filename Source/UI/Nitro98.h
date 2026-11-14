#pragma once

#include <JuceHeader.h>
#include "../Core/PhoqerTypes.h"

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

// ARP.EXE's step lanes: 16 steps on one grid, every other beat shaded, one tall lane that shows and edits each step's
// VEL (how loud) or GATE (how long, times the GATE knob), picked by the VEL | GATE switch under it. Drag to draw,
// right-click for a rest (or back), double-click for full again. The step sounding now is pink.
class ArpLanes98 final : public juce::Component
{
public:
    static constexpr int steps = phoqer::arpLaneSteps;
    std::function<phoqer::ArpLaneStep(int)> get;
    std::function<void(int, phoqer::ArpLaneStep)> set;
    std::function<void()> edited;      // a drag or click finished
    std::function<int()> playing;      // the step sounding now, -1 none
    bool gateMode = false;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    juce::Rectangle<int> cell(int step) const;     // a step's column, in this component
    int stepAt(int x) const;

private:
    void draw(const juce::MouseEvent&);
    int hover = -1;
    bool dragging = false;
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
