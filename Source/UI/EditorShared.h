#pragma once

// What the editor's files share: the window's layout, small formatting helpers and the menu-item helper.
// Each file that includes this gets its own private copy (an unnamed namespace), as one file had before.
#include "Editor.h"
#include "../Licence.h"

#include "Logo.h"
#include "Recorder.h"
#include "Settings.h"

#include <PhoqerUIAssets.h>

namespace phoqer::ui
{
namespace
{
// Window frames (view coordinates; KEYS.EXE is left out when hidden).
const juce::Rectangle<float> headWindow { 6, 4, 748, 90 };
const juce::Rectangle<float> modeWindow { 6, 100, 86, 278 };
const juce::Rectangle<float> sealWindow { 98, 100, 262, 278 };
const juce::Rectangle<float> scopeWindow { 366, 100, 388, 194 };
const juce::Rectangle<float> outputWindow { 366, 300, 388, 78 };
const juce::Rectangle<float> metroDefault { 465, 125, 173, 168 };    // METRO.EXE's first position: over SCOPE.EXE
const juce::Rectangle<float> mixerWindow { 6, 384, 748, 243 };
const juce::Rectangle<float> keysWindow { 6, 633, 748, 106 };

// The main window's title bar is taller than the others' so its real caption buttons are easy to hit.
constexpr float headTitleHeight = 22.0f;
constexpr float headerToolsHeight = 24.0f;
constexpr float captionButtonW = 20.0f, captionButtonH = 18.0f;


// Header: the PHOQER wordmark at 2 units per pixel like the rest of the UI, a little in from the left.
constexpr float logoWordPixel = 2.0f, logoWordInset = 16.0f;

// MANIC: the first 150 ms of a new call in BARK mode above this face intensity flashes the grin.
constexpr float manicIntensity = 0.85f;
constexpr double manicWatchSeconds = 0.15, manicHoldSeconds = 0.5;
constexpr int barkMode = 2;

const char* voiceNames[] { "BURP", "SQUEAL", "GROAN", "KYRAX" };

// NITRO.EXE's STACK: quick picks that fill in the three notes.
struct NitroStack { const char* name; int notes[3]; };
const NitroStack nitroStacks[] { { "OCT", { 12, 0, 0 } }, { "5TH + OCT", { 7, 12, 0 } }, { "SUB", { -12, 0, 0 } },
                                 { "SUB + OCT", { -12, 12, 0 } }, { "5TH OCT 12TH", { 7, 12, 19 } }, { "2 OCTAVES", { 12, 24, 0 } },
                                 { "SUB 5TH OCT", { -12, 7, 12 } }, { "4TH + OCT", { 5, 12, 0 } }, { "MINOR", { 3, 7, 0 } },
                                 { "MAJOR", { 4, 7, 0 } } };
// NITRO.EXE's size and its layout inside the client area: margin, the keyboard, the note rows, the dials.
constexpr float nitroWindowW = 274.0f, nitroWindowH = 272.0f;
constexpr float nitroMargin = 13.0f, nitroKeysY = 8.0f, nitroKeysH = 34.0f, nitroRowsY = 64.0f, nitroRowGap = 23.0f, nitroDialsY = 140.0f, nitroDialH = 64.0f;

juce::RangedAudioParameter& parameterFor(juce::AudioProcessorValueTreeState& state, const char* id)
{
    auto* p = state.getParameter(id);
    jassert(p != nullptr);
    return *p;
}

juce::String percent(double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; }
juce::String decibels(double v) { return (v > 0.0 ? "+" : "") + juce::String(v, 1) + " dB"; }
juce::String milliseconds(double v) { return v < 1000.0 ? juce::String(juce::roundToInt(v)) + " MS" : juce::String(v / 1000.0, 1) + " S"; }
juce::String noteName(int note) { return juce::MidiMessage::getMidiNoteName(note, true, true, 4); }
const char* const modeNames[] { "CALL", "HONK", "BARK", "WAIL", "MURMUR" };

constexpr char controlKeys[] { 'z', 'x', 'c', 'v' };    // octave down/up, velocity down/up
constexpr int lowestBase = 24, highestBase = 96, velocityStep = 10;

// Header row: the preset box sits between the logo and the voice tabs; REC at the menu row's end.
constexpr float presetBoxX = 224.0f, presetBoxW = 224.0f;

// Menu item IDs. Radio items (one of several) live in Win98LookAndFeel's radio range.
constexpr float zoomSteps[] { 1.0f, 1.25f, 1.5f, 2.0f };
constexpr int zoomIdFirst = Win98LookAndFeel::radioIdFirst + 1;
constexpr int presetIdFirst = Win98LookAndFeel::radioIdFirst + 100;

// The sound knobs RANDOMIZE touches (never OUTPUT, so it cannot jump your level).
const char* soundKnobs[] { "boom", "air", "bark", "space", "vowel", "detune", "tide" };

bool modifierHeld()
{
    const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
    return mods.isCtrlDown() || mods.isAltDown() || mods.isCommandDown();
}

juce::String minutesSeconds(double seconds)
{
    const int s = static_cast<int>(seconds);
    return juce::String(s / 60).paddedLeft('0', 2) + ":" + juce::String(s % 60).paddedLeft('0', 2);
}
}

namespace
{
void addItem(juce::PopupMenu& menu, const juce::String& text, int id, bool enabled, bool ticked, const juce::String& shortcut,
             std::function<void()> action)
{
    juce::PopupMenu::Item item(text);
    item.itemID = id;
    item.isEnabled = enabled;
    item.isTicked = ticked;
    item.shortcutKeyDescription = shortcut;
    item.action = std::move(action);
    menu.addItem(std::move(item));
}
}
}
