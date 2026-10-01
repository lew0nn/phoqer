#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "TestUiKnob.h"
#include "TestUiPortrait.h"
#include "TestUiViews.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace phoqer::testui
{
// What the PHOQER.EXE title bar can do to its window. The standalone app (TestUiStandaloneApp.cpp)
// supplies these for its frameless window; in a DAW none are set and the title bar has no buttons.
struct WindowControls
{
    std::function<void()> minimise, toggleFullscreen, close, audioSettings;
    std::function<void(const juce::MouseEvent&)> startDrag, drag;
};

// Experimental "Pure 98" editor, built only into the PHOQER Test UI target.
class TestUiEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int width = 760, height = 724;

    explicit TestUiEditor(PhoqerAudioProcessor&);
    ~TestUiEditor() override;

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

private:
    void timerCallback() override;
    void applyCharacter(int);
    void rebuildChrome(float scale);
    void setChoice(const char* id, int index);
    void releaseQwertyNotes();
    bool onTitleBar(juce::Point<float>) const;

    PhoqerAudioProcessor& processor;
    PortraitRenderer portrait;
    SealView seal;
    ScopeView scope;
    MeterView meter;
    MenuBar98 menuBar;
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

    int character = 1;
    juce::Image chrome, logoIcon, logoWord;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestUiEditor)
};
}
