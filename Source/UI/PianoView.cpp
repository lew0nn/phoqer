// KEYS.EXE: the on-screen keyboard, with the computer keys printed on it.

#include "Views.h"

#include "../Core/FishSprite.h"

#include "Style.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace phoqer::ui
{
// ---------------------------------------------------------------------------------------- PIANO
PianoView::PianoView(juce::MidiKeyboardState& state) : juce::MidiKeyboardComponent(state, horizontalKeyboard)
{
    // The editor owns computer-keyboard playing; this component only handles the mouse.
    clearKeyMappings();
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    setScrollButtonsVisible(false);
    setBlackNoteLengthProportion(0.6f);
    setBlackNoteWidthProportion(0.62f);
    setVelocity(0.9f, true);
    setOctaveForMiddleC(4);
    setColour(whiteNoteColourId, win98::dark);                  // shows through the 1 px gaps between keys
    setColour(keySeparatorLineColourId, win98::dark);
    setColour(shadowColourId, juce::Colours::transparentBlack);
    setQwertyBase(60);
}

void PianoView::setQwertyBase(int note)
{
    qwertyBase = note;
    setAvailableRange(note - 12, note + 24);
    resized();
    repaint();
}

void PianoView::resized()
{
    if (getWidth() > 0)
        setKeyWidth(static_cast<float>(getWidth()) / static_cast<float>(whiteKeysShown));
    setLowestVisibleKey(qwertyBase - 12);
    juce::MidiKeyboardComponent::resized();
}

juce::String PianoView::keyLabel(int note) const
{
    if (! qwertyLabels) return {};
    const int i = note - qwertyBase;
    return juce::isPositiveAndBelow(i, qwertyKeyCount) ? juce::String::charToString(qwertyKeys[i]).toUpperCase() : juce::String();
}

void PianoView::drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over, juce::Colour, juce::Colour)
{
    const auto& pal = paletteFor(character);
    const auto key = area.withTrimmedRight(1.0f);
    g.setColour(down ? pal.accent : over ? juce::Colour(0xffe4e4ec) : juce::Colour(0xfff2f2f2));
    g.fillRect(key);
    if (! down)
    {
        g.setColour(win98::face);                              // pixel bevel at the key's base
        g.fillRect(key.withTop(key.getBottom() - 4.0f));
        g.setColour(win98::shadow);
        g.fillRect(key.withTop(key.getBottom() - 2.0f));
    }
    const auto ink = down ? juce::Colours::white : juce::Colours::black;
    if (const auto label = keyLabel(note); label.isNotEmpty())
        drawText(g, label, key.withTop(key.getBottom() - 18.0f).withTrimmedBottom(5.0f), pixelFont(9.0f, true), ink, juce::Justification::centred);
    if (note % 12 == 0)
        drawText(g, "C" + juce::String(note / 12 - 1), key.withTop(key.getBottom() - 30.0f).withHeight(10.0f), pixelFont(7.0f),
                 ink.withAlpha(0.55f), juce::Justification::centred);
}

void PianoView::drawBlackNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over, juce::Colour)
{
    const auto& pal = paletteFor(character);
    g.setColour(win98::dark);
    g.fillRect(area);
    const auto top = area.reduced(1.0f, 0.0f).withTrimmedBottom(1.0f);
    g.setColour(down ? pal.accent.brighter(0.2f) : over ? juce::Colour(0xff4a4458) : juce::Colour(0xff2a2632));
    g.fillRect(top);
    if (! down)
    {
        g.setColour(juce::Colour(0xff5c5868));                 // raised highlight on the key face
        g.fillRect(top.withWidth(2.0f).withTrimmedBottom(4.0f));
    }
    if (const auto label = keyLabel(note); label.isNotEmpty())
        drawText(g, label, top.withTop(top.getBottom() - 14.0f), pixelFont(8.0f, true),
                 down ? juce::Colours::black : juce::Colours::white.withAlpha(0.8f), juce::Justification::centred);
}
}
