#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "TestUiKnob.h"
#include "TestUiPortrait.h"
#include "TestUiViews.h"

#include <array>
#include <memory>
#include <vector>

namespace phoqer::testui
{
// Experimental "Pure 98" editor, built only into the PHOQER Test UI target.
class TestUiEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int width = 760, height = 754;

    explicit TestUiEditor(PhoqerAudioProcessor&);
    ~TestUiEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;

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

    PhoqerAudioProcessor& processor;
    PortraitRenderer portrait;
    SealView seal;
    ScopeView scope;
    MeterView meter;
    MenuBar98 menuBar;
    Taskbar98 taskbar;
    PianoView piano;
    std::array<std::unique_ptr<ChoiceButton>, 3> voiceTabs;
    std::array<std::unique_ptr<ChoiceButton>, 5> modeButtons;
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
