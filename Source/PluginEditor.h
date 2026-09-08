#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "UI/PhoqerComponents.h"
#include "UI/PhoqerLookAndFeel.h"
#include "UI/PhoqerTheme.h"

#include <array>
#include <memory>

class PhoqerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit PhoqerAudioProcessorEditor(PhoqerAudioProcessor&);
    ~PhoqerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void applyVisualForCharacter(int);
    void showMenu();
    void showVoiceMenu();
    void setChoice(const char* parameterId, int index);
    static void prepareRotary(juce::Slider&, const juce::String& name);

    PhoqerAudioProcessor& processor;
    phoqer::ui::ThemeStore themeStore;
    const phoqer::ui::ThemeAssets* theme = nullptr;
    int characterIndex = 1;
    phoqer::ui::PhoqerLookAndFeel lookAndFeel;

    std::array<juce::Slider, 4> macroSliders;
    juce::Slider tideSlider;
    std::array<std::unique_ptr<SliderAttachment>, 4> macroAttachments;
    std::unique_ptr<SliderAttachment> tideAttachment;

    phoqer::ui::SealDisplay sealDisplay;
    phoqer::ui::WaveformDisplay waveformDisplay;
    phoqer::ui::OutputMeter outputMeter;
    std::array<std::unique_ptr<phoqer::ui::ModeButton>, 5> modeButtons;
    phoqer::ui::MenuButton menuButton;

    juce::ParameterAttachment characterAttachment;
    juce::TooltipWindow tooltip { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhoqerAudioProcessorEditor)
};
