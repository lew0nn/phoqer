#pragma once

#include <JuceHeader.h>

#include "../Core/FaceTelemetry.h"
#include "Portrait.h"

#include <array>
#include <functional>

namespace phoqer::ui
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
    void showSpectrogram(double seconds);       // the fed seal's song draws a fish; this shows it

private:
    void updateSpectrogram();
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
    int spectrogramFrames = 0;
    juce::Image spectrogram;
    uint32_t lastColumn = 0;                  // the newest spectrogram column already drawn
    std::vector<float> fftWork, hann;
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
    std::function<void()> onReselect;     // set: a click while selected, or a right-click, calls this instead

private:
    void clicked(const juce::ModifierKeys& m) override { popupClick = m.isPopupMenu(); }
    bool popupClick = false;
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
    void setQwertyLabels(bool on) { qwertyLabels = on; repaint(); }     // the computer-key letters (standalone)

private:
    void drawWhiteNote(int note, juce::Graphics&, juce::Rectangle<float>, bool down, bool over, juce::Colour, juce::Colour) override;
    void drawBlackNote(int note, juce::Graphics&, juce::Rectangle<float>, bool down, bool over, juce::Colour) override;
    juce::String getWhiteNoteText(int) override { return {}; }
    juce::String keyLabel(int note) const;
    void resized() override;

    int character = 1, qwertyBase = 60;
    bool qwertyLabels = false;
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
    void setState(bool recording, double seconds, int countInBeats = 0);     // during a count-in: beats left
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> buttonBounds() const;
    bool recording = false;
    int shownSeconds = 0, shownCount = 0;
};

// The secret behind the header wordmark: double-click the wordmark and it slides aside to show this
// fish. Pick it up and drag it anywhere; let go and it flops back home.
class Fish98 final : public juce::Component, private juce::Timer
{
public:
    static constexpr int pixel = 2, spriteW = 20, spriteH = 10;
    Fish98();
    void setCharacter(int);
    void setHome(juce::Point<int> topLeft);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void visibilityChanged() override;
    std::function<bool(juce::Point<int>)> onDrop;     // where it was let go, in the parent; true if eaten
    bool isHeld() const noexcept { return held; }

private:
    void timerCallback() override;
    std::array<juce::Image, 2> frames;      // tail straight, tail flicked
    juce::Point<int> home;
    juce::ComponentDragger dragger;
    bool held = false, returning = false;
    int frame = 0, ticks = 0;
};

// Overfed: SEAL.SYS crashes the whole window, Win98 style. Any key or click reboots the seal.
class BlueScreen98 final : public juce::Component, private juce::Timer
{
public:
    BlueScreen98();
    std::function<void()> onDismiss;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override { if (onDismiss) onDismiss(); }

private:
    void timerCallback() override { cursorOn = ! cursorOn; repaint(); }
    bool cursorOn = true;
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

// Lets a pop-up inside the editor be dragged out into a window of its own, anywhere on screen, at the
// editor's zoom. Let go of it wholly over the editor and it docks again. area: the part of the component
// that is the window (all of it, or a message box's box), in its own coordinates. The owner forwards the
// mouse-down and drags on its grab area (a title bar), and places the area for moveArea.
class PopOut98 final : private juce::Timer
{
public:
    PopOut98(juce::Component& owner, std::function<juce::Rectangle<int>()> area);
    ~PopOut98() override;
    bool isOut() const noexcept { return holder != nullptr; }
    void mouseDown(const juce::MouseEvent&);
    void mouseDrag(const juce::MouseEvent&);
    void dock();
    std::function<void(juce::Point<int>)> moveArea;      // put the area's top-left here, in the parent's coordinates
    std::function<void(bool out)> onChange;               // popped out, or docked again

private:
    void popOut(juce::Point<int> mouseOnScreen);
    void timerCallback() override;     // while out: follows the mouse during a drag; on top while PHOQER's app is in front
    juce::Component& owner;
    std::function<juce::Rectangle<int>()> area;
    std::unique_ptr<juce::Component> holder;     // the desktop window it sits in while out
    juce::Component* home = nullptr;             // the editor view it docks back into
    juce::Point<float> grab;                     // where it was grabbed, from the area's top-left, in editor units
    float scale = 1.0f;                          // the editor's zoom, carried out with it
    bool following = false, onTop = false;
};

// A Win98 window floating inside the editor: drag it by its title bar, X hides it. Content goes in
// clientArea() as child components; paintContent draws the rest (labels, lights) under them.
// It can be dragged out of the editor (PopOut98); closing it docks it again.
class FloatingWindow98 final : public juce::Component
{
public:
    explicit FloatingWindow98(const juce::String& title);
    ~FloatingWindow98() override;
    bool isPoppedOut() const noexcept { return popOut.isOut(); }
    void dock() { popOut.dock(); }
    void visibilityChanged() override;
    void setCharacter(int c) { character = c; repaint(); }
    juce::Rectangle<float> clientArea() const;
    std::function<void()> onClose, onMoved;
    std::function<juce::Rectangle<float>(juce::Graphics&,juce::Rectangle<float>)> paintFrame;
    std::function<void(juce::Graphics&, juce::Rectangle<float> client)> paintContent;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::String title;
    int character = 0;
    bool dragging = false;
    TitleButton98 close { TitleButton98::Kind::close };
    PopOut98 popOut { *this, [this] { return getLocalBounds(); } };
};

// WAIL BEND: a small raised panel beside the WAIL button. A slider from 0 to 24 semitones and the value,
// which a double-click turns into a box to type in. Any click outside closes it.
class WailBendPanel98 final : public juce::Component
{
public:
    WailBendPanel98(juce::RangedAudioParameter& bend, juce::RangedAudioParameter& hold);
    ~WailBendPanel98() override;
    void setCharacter(int);
    void open(juce::Rectangle<int> beside, juce::Component* opener);     // clicks on the opener are not "outside"
    void close();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override { popOut.mouseDown(e); }     // drag it by its background
    void mouseDrag(const juce::MouseEvent& e) override { popOut.mouseDrag(e); }
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label value;
    struct Padlock final : juce::Button     // pressed in: RISE AND HOLD
    {
        Padlock() : juce::Button("WAIL HOLD") { setClickingTogglesState(true); }
        void paintButton(juce::Graphics&, bool over, bool down) override;
    } lock;
    std::function<void()> onBeginEdit;

private:
    struct Outside final : juce::MouseListener     // listens to every click while open: one outside closes the panel
    {
        explicit Outside(WailBendPanel98& p) : panel(p) {}
        void mouseDown(const juce::MouseEvent&) override;
        WailBendPanel98& panel;
    } outside { *this };
    void show(float);
    juce::SliderParameterAttachment attachment;
    juce::ButtonParameterAttachment lockAttachment;
    juce::Component* opener = nullptr;
    int character = 0;
    PopOut98 popOut { *this, [this] { return getLocalBounds(); } };
};

// A small LCD readout that is also a button: the BPM, which opens METRO.EXE.
class Readout98 final : public juce::Button
{
public:
    Readout98() : juce::Button("READOUT") {}
    void setCharacter(int c) { character = c; repaint(); }
    void setText(const juce::String& t) { if (t != shown) { shown = t; repaint(); } }
    void paintButton(juce::Graphics&, bool over, bool down) override;

private:
    juce::String shown;
    int character = 0;
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

// A typed number on a glass in the voice's colours, bound to a parameter, its label at the left: the BPM in
// METRO.EXE. Click to type; Enter applies.
class NumberField98 final : public juce::Component, public juce::SettableTooltipClient
{
public:
    NumberField98(juce::RangedAudioParameter&, const juce::String& label, int minimum, int maximum, float textSize);
    void setCharacter(int);
    void paint(juce::Graphics&) override;
    void resized() override;
    std::function<void()> onBeginEdit;

private:
    juce::Rectangle<int> fieldArea() const;
    void show(float v);
    juce::Label field;
    juce::String label;
    int minimum, maximum;
    float textSize;
    int character = 0;
    float value = 0.0f;
    juce::ParameterAttachment attachment;
};

// The metronome switch: a pressed-in button with a wireframe metronome on dark glass, in the voice's
// colours. While the click is on, its arm swings left and right, reaching an end on every beat.
class MetronomeButton98 final : public juce::Button
{
public:
    MetronomeButton98();
    void setCharacter(int c) { character = c; repaint(); }
    std::function<float()> beats;      // where the metronome is, in beats since it was switched on
    void paintButton(juce::Graphics&, bool over, bool down) override;

private:
    int character = 0;
};

// The dice beside the preset name: one click shuffles the sound (EDIT > RANDOMIZE KNOBS). A neon cube on the
// metronome's dark glass, in the voice's colours: 1 on top, 2 on the left, 3 on the right.
class DiceButton98 final : public juce::Button
{
public:
    DiceButton98();
    void setCharacter(int c) { character = c; repaint(); }
    void paintButton(juce::Graphics&, bool over, bool down) override;

private:
    int character = 0;
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
    void mouseDown(const juce::MouseEvent&) override;     // drag the box by its title bar, out of the editor too
    void mouseDrag(const juce::MouseEvent&) override;
    juce::Rectangle<float> boxBounds() const;

private:
    void finish(int button);
    float linesHeight() const;

    Spec spec;
    juce::Point<float> offset;                 // where the box was dragged to, from the centre
    bool dragging = false;
    juce::Component blocker;                   // while the box is out, still blocks clicks on the editor
    PopOut98 popOut { *this, [this] { return boxBounds().expanded(0.0f, 0.0f).withTrimmedRight(-4.0f).withTrimmedBottom(-4.0f).toNearestInt(); } };
    juce::TextEditor field;
    TitleButton98 closeButton { TitleButton98::Kind::close };
    std::vector<std::unique_ptr<PushButton98>> buttons;
    bool finished = false;
};
}
