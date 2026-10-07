#pragma once

#include <JuceHeader.h>

#include "../Core/FaceTelemetry.h"
#include "TestUiPortrait.h"

#include <array>
#include <functional>

namespace phoqer::testui
{
// SEAL.BMP client area: the expressive portrait bitmap, scanlines and call-driven glitch slices.
class SealView final : public juce::Component
{
public:
    void update(const PortraitRenderer&, int character, const Expression&);
    void paint(juce::Graphics&) override;

private:
    juce::Image bitmap;
    float intensity = 0.0f;
    int character = 0;
};

// SCOPE.EXE client area: captures the last call from the telemetry ring and draws it as pixel columns.
class ScopeView final : public juce::Component
{
public:
    explicit ScopeView(const TelemetryPublisher&);
    void setCharacter(int c) { character = c; repaint(); }
    void refresh();
    void paint(juce::Graphics&) override;
    float capturedMilliseconds() const noexcept { return capturedMs; }

private:
    enum class CaptureState { idle, capturing, holding, decaying };
    const TelemetryPublisher& telemetry;
    static constexpr int bucketCount = 1200;
    std::array<float, bucketCount> positive {}, negative {};
    std::array<float, TelemetryPublisher::waveformSize> capture {};
    uint32_t lastWriteIndex = 0, lastCallSerial = 0;
    int usedBuckets = 0, samplesInBucket = 0, samplesPerBucket = 48;
    double silentSeconds = 0.0, holdSeconds = 0.0;
    float displayGain = 1.0f, traceOpacity = 0.0f, capturePeak = 0.0f, capturedMs = 0.0f;
    CaptureState captureState = CaptureState::idle;
    int character = 0;
};

// OUTPUT meter: chunky segment bar with a dB scale.
class MeterView final : public juce::Component
{
public:
    explicit MeterView(const TelemetryPublisher& t) : telemetry(t) {}
    void setCharacter(int c) { character = c; repaint(); }
    void refresh();
    void paint(juce::Graphics&) override;

private:
    const TelemetryPublisher& telemetry;
    float peak = 0.0f;
    int character = 0;
};

// A Win98 push button bound to a choice parameter value (voice tab or mode button).
class ChoiceButton final : public juce::Button
{
public:
    enum class Kind { voiceTab, mode };
    ChoiceButton(Kind, int index, const juce::String& label, juce::RangedAudioParameter&);
    void setCharacter(int c) { character = c; repaint(); }
    void paintButton(juce::Graphics&, bool over, bool down) override;
    bool isSelected() const noexcept { return selected; }

private:
    Kind kind;
    int index;
    bool selected = false;
    int character = 0;
    juce::ParameterAttachment attachment;
};

// Computer-keyboard layout shared by the editor and the piano labels: one row plays 17 semitones
// up from the base note, like a DAW's typing keyboard.
inline constexpr const char* qwertyKeys = "awsedftgyhujkolp;";
inline constexpr int qwertyKeyCount = 17;

// KEYS.EXE client area: a clickable Win98 piano bound to the processor's keyboard state. Keys are
// labelled with the computer key that plays them; notes from MIDI input light up too.
class PianoView final : public juce::MidiKeyboardComponent
{
public:
    static constexpr int whiteKeysShown = 22;    // three octaves plus the top C
    explicit PianoView(juce::MidiKeyboardState&);
    void setCharacter(int c) { character = c; repaint(); }
    void setQwertyBase(int note);                // shows an octave below to two above the base note

private:
    void drawWhiteNote(int note, juce::Graphics&, juce::Rectangle<float>, bool down, bool over, juce::Colour, juce::Colour) override;
    void drawBlackNote(int note, juce::Graphics&, juce::Rectangle<float>, bool down, bool over, juce::Colour) override;
    juce::String getWhiteNoteText(int) override { return {}; }
    juce::String keyLabel(int note) const;
    void resized() override;

    int character = 1, qwertyBase = 60;
};

// Win98 menu bar: EDIT, VIEW, PRESET, HELP. It only draws the titles (the open one pressed in) and
// reports clicks; the editor builds each menu. The underlined first letter opens it with Alt.
class MenuBar98 final : public juce::Component
{
public:
    static constexpr const char* items[] { "EDIT", "VIEW", "PRESET", "HELP" };
    static constexpr int itemCount = 4;

    std::function<void(int)> onOpen;
    void setOpenIndex(int index);
    juce::Rectangle<float> itemBounds(int index) const;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    int openIndex = -1;
};

// Small Win98 button with a pixel arrow, for stepping through presets.
class ArrowButton98 final : public juce::Button
{
public:
    explicit ArrowButton98(bool pointsLeft);
    void paintButton(juce::Graphics&, bool over, bool down) override;

private:
    bool left;
};

// The header's preset box: the current preset's number and name in a sunken field (a star when it
// has been changed), with previous / next arrows. Clicking the field opens the preset list.
class PresetBox98 final : public juce::Component, public juce::SettableTooltipClient
{
public:
    PresetBox98();
    std::function<juce::String()> text;
    std::function<void()> onPrevious, onNext, onOpenList;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> fieldBounds() const;
    ArrowButton98 previous { true }, next { false };
};

// REC button and running time, at the right end of the menu-bar row (standalone app only).
class RecordControl98 final : public juce::Component, public juce::SettableTooltipClient
{
public:
    RecordControl98();
    std::function<void()> onToggle;
    void setState(bool recording, double seconds);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> buttonBounds() const;
    bool recording = false;
    int shownSeconds = 0;
};

// A working Win98 title-bar button: minimise, maximise (fullscreen) or close.
class TitleButton98 final : public juce::Button
{
public:
    enum class Kind { minimise, maximise, close };
    explicit TitleButton98(Kind);
    void paintButton(juce::Graphics&, bool over, bool down) override;

private:
    Kind kind;
};

// A Win98 push button with a text label; the default button gets the black outer border.
class PushButton98 final : public juce::Button
{
public:
    PushButton98(const juce::String& label, bool isDefault);
    void paintButton(juce::Graphics&, bool over, bool down) override;

private:
    bool isDefault;
};

// A Win98 message box drawn inside the editor (so it scales with the UI and works inside a DAW):
// title bar, optional icon, message lines, optional one-line text field, and buttons. It covers its
// parent to block clicks underneath. Enter presses the first button, Escape or X closes with -1
// (unless the box is not closable: then only its buttons end it).
class Dialog98 final : public juce::Component
{
public:
    struct Spec
    {
        juce::String title;
        juce::StringArray lines;
        juce::Image icon;                                  // optional, shown at 2 units per pixel
        juce::String fieldLabel, fieldText;                // a text field when fieldLabel is set
        juce::StringArray buttons { "OK" };
        int character = 0;
        bool closable = true;                              // false: no X, Escape does nothing
        float width = 360.0f;                              // wider for long lines (guide, shortcuts)
        float labelWidth = 150.0f;                         // the left column of "NAME|text" lines
        std::function<void(int button, const juce::String& field)> onClose;
    };

    // Lines: plain text; "~text" in grey; "#HEADING" a section heading with an etched rule;
    // "NAME|text" two columns, the name in the voice colour; "[CTRL]+[Z]|text" the keys as keycaps.
    explicit Dialog98(Spec);
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void parentHierarchyChanged() override;
    juce::Rectangle<float> boxBounds() const;

private:
    void finish(int button);
    float linesHeight() const;

    Spec spec;
    juce::TextEditor field;
    TitleButton98 closeButton { TitleButton98::Kind::close };
    std::vector<std::unique_ptr<PushButton98>> buttons;
    bool finished = false;
};
}
