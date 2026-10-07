#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "History.h"
#include "Knob.h"
#include "Portrait.h"
#include "Presets.h"
#include "Style.h"
#include "Views.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace phoqer::ui
{
// What the PHOQER.EXE title bar can do to its window. The standalone app (StandaloneApp.cpp)
// supplies these for its frameless window; in a DAW none are set and the title bar has no buttons.
struct WindowControls
{
    std::function<void()> minimise, toggleFullscreen, close, audioSettings;
    std::function<void(const juce::MouseEvent&)> startDrag, drag;
};

// The editor itself, laid out at a fixed base size (760 wide; shorter when KEYS.EXE is hidden).
// PhoqerEditor scales it to the chosen zoom.
class PhoqerView final : public juce::Component, private juce::Timer
{
public:
    static constexpr int width = 760, fullHeight = 730, compactHeight = 618;

    PhoqerView(PhoqerAudioProcessor&, Win98LookAndFeel&);
    ~PhoqerView() override;

    int baseHeight() const noexcept { return showKeys ? fullHeight : compactHeight; }

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void setWindowControls(WindowControls);

    // One UI frame: reads telemetry, animates the face, refreshes the views. Driven by the timer,
    // and called directly by the offline acceptance renderer.
    void tick(double nowSeconds);
    void forceManicForTest(double nowSeconds) { manicUntil = nowSeconds + 0.5; }

    // VIEW menu actions, carried out by the editor that scales this view.
    std::function<float()> currentZoom;
    std::function<void(float)> onZoom;
    std::function<void()> onFitToScreen, onLayoutChanged;

private:
    void timerCallback() override;
    void applyCharacter(int);
    void rebuildChrome(float scale);
    void setChoice(const char* id, int index);
    void releaseQwertyNotes();
    bool onTitleBar(juce::Point<float>) const;
    bool isStandalone() const noexcept { return windowControls.audioSettings != nullptr; }

    // Menus and the actions behind them.
    void showMenu(int index);
    void showPopup(juce::PopupMenu&, juce::Rectangle<float> target, int menuIndex);
    juce::PopupMenu buildMenu(int index);
    void addPresetItems(juce::PopupMenu&);
    void resetKnobs();
    void randomizeKnobs();
    void stepPreset(int delta);
    void loadPreset(int index);
    void toggleKeys();
    void showAbout();
    void showShortcuts();
    void showGuide();
    void showLicences();
    void panic();
    void copySettings();
    void pasteSettings();
    void showSavePreset();
    void savePresetAs(const juce::String& name);
    void showOpenPreset();
    void toggleRecording();
    void saveTake(const juce::String& fileName);
    void showDialog(Dialog98::Spec);
    void showMessage(const juce::String& title, const juce::StringArray& lines);

    PhoqerAudioProcessor& processor;
    Win98LookAndFeel& lookAndFeel;
    PortraitRenderer portrait;
    SealView seal;
    ScopeView scope;
    MeterView meter;
    MenuBar98 menuBar;
    PresetBox98 presetBox;
    RecordControl98 recordControl;
    PianoView piano;
    std::array<std::unique_ptr<ChoiceButton>, 3> voiceTabs;
    std::array<std::unique_ptr<ChoiceButton>, 5> modeButtons;
    std::array<std::unique_ptr<TitleButton98>, 3> titleButtons;    // minimise, maximise, close
    WindowControls windowControls;
    bool draggingWindow = false;
    std::vector<std::unique_ptr<PixelKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    juce::ParameterAttachment characterAttachment;
    juce::TooltipWindow tooltips { this, 700 };

    PresetLibrary presets;
    ParamHistory history;
    std::unique_ptr<Dialog98> dialog;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File pendingTake;                                         // a stopped recording awaiting SAVE / DISCARD
    int recordChannels = 2;
    juce::String shownPreset;

    int character = 0;
    bool showKeys = true;
    juce::Image chrome, logoWord;
    float chromeScale = 0.0f;
    FaceTelemetry face {};
    float grin = 0.0f;
    double manicUntil = 0.0, watchUntil = 0.0;
    uint32_t lastCallSerial = 0;
    int frame = 0;

    // Computer-keyboard playing: base note of the A key, velocity, and the note each key is holding.
    int qwertyBase = 60, qwertyVelocity = 120;
    std::array<int, qwertyKeyCount> heldNotes;
    std::array<bool, 4> controlKeysDown {};    // Z X C V

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhoqerView)
};

// PHOQER's "Pure 98" editor: the view above, scaled
// to the saved zoom, with a Win98 size grip. The zoom is kept in UiSettings for the next launch.
class PhoqerEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PhoqerEditor(PhoqerAudioProcessor&);
    ~PhoqerEditor() override;

    void resized() override;

    void setWindowControls(WindowControls c) { view.setWindowControls(std::move(c)); }
    void tick(double nowSeconds) { view.tick(nowSeconds); }
    void forceManicForTest(double nowSeconds) { view.forceManicForTest(nowSeconds); }
    PhoqerView& getView() noexcept { return view; }

    static constexpr float minZoom() noexcept { return 0.75f; }
    static constexpr float maxZoom() noexcept { return 4.0f; }
    void setZoom(float);
    float getZoom() const noexcept { return zoom; }
    void fitToScreen();

private:
    void updateLimits();
    juce::Rectangle<int> screenArea() const;

    Win98LookAndFeel laf;
    PhoqerView view;
    float zoom = 1.0f;
    bool remembersZoom = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhoqerEditor)
};
}
