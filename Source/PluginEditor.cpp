#include "PluginEditor.h"

#include "UI/PhoqerLayout.h"

namespace
{
juce::RangedAudioParameter& parameter(juce::AudioProcessorValueTreeState& state, const char* id)
{
    auto* result = state.getParameter(id);
    jassert(result != nullptr);
    return *result;
}
}

PhoqerAudioProcessorEditor::PhoqerAudioProcessorEditor(PhoqerAudioProcessor& owner)
    : AudioProcessorEditor(owner), processor(owner),
      sealDisplay(owner.getTelemetry()), waveformDisplay(owner.getTelemetry()),
      outputMeter(owner.getTelemetry()),
      characterAttachment(parameter(owner.getParameters(), "character"),
                          [this](float value)
                          {
                              applyVisualForCharacter(juce::roundToInt(value));
                          }, nullptr)
{
    setOpaque(true);
    setResizable(false, false);

    constexpr const char* ids[] { "boom", "air", "bark", "space" };
    constexpr const char* names[] { "BOOM", "AIR", "BARK", "SPACE" };
    for (size_t index = 0; index < macroSliders.size(); ++index)
    {
        prepareRotary(macroSliders[index], names[index]);
        addAndMakeVisible(macroSliders[index]);
        macroAttachments[index] = std::make_unique<SliderAttachment>(
            owner.getParameters(), ids[index], macroSliders[index]);
    }

    tideSlider.setName("TIDE");
    tideSlider.setSliderStyle(juce::Slider::LinearVertical);
    tideSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    tideSlider.setRange(0.0, 1.0, 0.001);
    tideSlider.setTooltip("TIDE: motion and modulation depth");
    addAndMakeVisible(tideSlider);
    tideAttachment = std::make_unique<SliderAttachment>(owner.getParameters(), "tide", tideSlider);

    auto& behavior = parameter(owner.getParameters(), "behavior");
    for (size_t index = 0; index < modeButtons.size(); ++index)
    {
        modeButtons[index] = std::make_unique<phoqer::ui::ModeButton>(static_cast<int>(index), behavior);
        addAndMakeVisible(*modeButtons[index]);
    }
    // The supplied first sidebar glyph is the VOICE control.  It owns the
    // single sound-character selector; the hamburger remains configuration-only.
    modeButtons.front()->onClick = [this] { showVoiceMenu(); };

    addAndMakeVisible(sealDisplay);
    addAndMakeVisible(waveformDisplay);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(menuButton);
    menuButton.onClick = [this] { showMenu(); };

    setLookAndFeel(&lookAndFeel);
    characterAttachment.sendInitialUpdate();
    setSize(phoqer::ui::Layout::width, phoqer::ui::Layout::height);
    startTimerHz(60);
}

PhoqerAudioProcessorEditor::~PhoqerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PhoqerAudioProcessorEditor::prepareRotary(juce::Slider& slider, const juce::String& name)
{
    slider.setName(name);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
    slider.setRange(0.0, 1.0, 0.001);
    slider.setTooltip(name);
}

void PhoqerAudioProcessorEditor::applyVisualForCharacter(int index)
{
    characterIndex = juce::jlimit(0, 2, index);
    theme = &themeStore.get(phoqer::ui::visualStyleForCharacter(characterIndex));
    lookAndFeel.setTheme(*theme);
    sealDisplay.setTheme(*theme);
    waveformDisplay.setTheme(*theme);
    outputMeter.setTheme(*theme);
    menuButton.setTheme(*theme);
    for (auto& button : modeButtons)
        if (button != nullptr)
            button->setTheme(*theme);
    repaint();
}

void PhoqerAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (theme == nullptr)
        return;

    juce::ColourGradient shell(theme->shellTop, 0.0f, 0.0f, theme->shellBottom,
                               0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(shell);
    g.fillRect(getLocalBounds());

    g.setColour(theme->metalLight.withAlpha(0.65f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f, 1.2f);

    const auto header = phoqer::ui::Layout::header();
    const auto controls = phoqer::ui::Layout::controlBand();
    g.setColour(theme->hairline.withAlpha(0.55f));
    g.drawHorizontalLine(header.getBottom(), static_cast<float>(header.getX()),
                         static_cast<float>(header.getRight()));
    g.drawHorizontalLine(controls.getY() - phoqer::ui::Layout::sectionGap / 2,
                         static_cast<float>(controls.getX()),
                         static_cast<float>(controls.getRight()));

    g.setColour(theme->text);
    g.setFont(juce::FontOptions(27.0f));
    auto titleArea = header;
    titleArea.removeFromRight(260);
    g.drawText("PHOQER", titleArea, juce::Justification::centredLeft, false);

    g.setColour(theme->textMuted);
    g.setFont(juce::FontOptions(9.0f));
    auto subtitleArea = header.reduced(210, 0);
    g.drawText("VOCAL  /  BARK  /  CHANT", subtitleArea,
               juce::Justification::centred, false);

    constexpr const char* labels[] { "BOOM", "AIR", "BARK", "SPACE" };
    g.setColour(theme->text);
    g.setFont(juce::FontOptions(11.0f));
    for (int index = 0; index < 4; ++index)
        g.drawText(labels[index], phoqer::ui::Layout::knobLabel(index),
                   juce::Justification::centred, false);
    g.drawText("TIDE", phoqer::ui::Layout::tideLabel(),
               juce::Justification::centred, false);
}

void PhoqerAudioProcessorEditor::resized()
{
    for (int index = 0; index < 4; ++index)
        macroSliders[static_cast<size_t>(index)].setBounds(phoqer::ui::Layout::knob(index));
    tideSlider.setBounds(phoqer::ui::Layout::tide());
    sealDisplay.setBounds(phoqer::ui::Layout::seal());
    waveformDisplay.setBounds(phoqer::ui::Layout::waveform());
    outputMeter.setBounds(phoqer::ui::Layout::meter());
    menuButton.setBounds(phoqer::ui::Layout::menu());
    for (int index = 0; index < 5; ++index)
        modeButtons[static_cast<size_t>(index)]->setBounds(phoqer::ui::Layout::modeButton(index));

    jassert(phoqer::ui::Layout::knob(0).getCentreY()
            == phoqer::ui::Layout::knob(3).getCentreY());
    jassert(phoqer::ui::Layout::knobLabel(0).getY()
            == phoqer::ui::Layout::knobLabel(3).getY());
    jassert(phoqer::ui::Layout::waveform().getX() == phoqer::ui::Layout::meter().getX());
    jassert(phoqer::ui::Layout::waveform().getRight() == phoqer::ui::Layout::meter().getRight());
    jassert(getLocalBounds().contains(phoqer::ui::Layout::tide()));
}

void PhoqerAudioProcessorEditor::timerCallback()
{
    waveformDisplay.refresh();
    outputMeter.refresh();
    sealDisplay.refresh();
}

void PhoqerAudioProcessorEditor::setChoice(const char* parameterId, int index)
{
    auto& choice = parameter(processor.getParameters(), parameterId);
    const auto normalized = choice.convertTo0to1(static_cast<float>(index));
    choice.beginChangeGesture();
    choice.setValueNotifyingHost(normalized);
    choice.endChangeGesture();
}

void PhoqerAudioProcessorEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.addItem(300, "PHOQER 0.1.0", false, false);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton),
                       [](int) {});
}

void PhoqerAudioProcessorEditor::showVoiceMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("VOICE");
    menu.addItem(101, "SQUEAL", true, characterIndex == 1);
    menu.addItem(102, "BURP", true, characterIndex == 0);
    menu.addItem(103, "GROAN", true, characterIndex == 2);

    const auto safeThis = juce::Component::SafePointer<PhoqerAudioProcessorEditor>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(modeButtons.front().get()),
                       [safeThis](int result)
                       {
                           if (safeThis == nullptr || result == 0)
                               return;
                           if (result == 101)
                               safeThis->setChoice("character", 1);
                           else if (result == 102)
                               safeThis->setChoice("character", 0);
                           else if (result == 103)
                               safeThis->setChoice("character", 2);
                       });
}
