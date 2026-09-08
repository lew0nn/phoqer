#include "../PluginProcessor.h"
#include "../UI/PhoqerComponents.h"
#include <iostream>

namespace {
void save(juce::Component& component, const juce::File& output)
{
    auto image = component.createComponentSnapshot(component.getLocalBounds(), true, 2.0f);
    juce::FileOutputStream stream(output);
    if (!stream.openedOk() || !juce::PNGImageFormat().writeImageToStream(image, stream))
        throw std::runtime_error("Cannot save acceptance render");
}
void refresh(juce::Component& editor)
{
    for (auto* child : editor.getChildren())
    {
        if (auto* wave = dynamic_cast<phoqer::ui::WaveformDisplay*>(child)) wave->refresh();
        if (auto* meter = dynamic_cast<phoqer::ui::OutputMeter*>(child)) meter->refresh();
    }
}
void set(PhoqerAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getParameters().getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc != 2) return 2;
    const juce::File folder(juce::String::fromUTF8(argv[1]));
    folder.createDirectory();
    for (int character : {1, 0, 2})
    {
        auto processor = std::make_unique<PhoqerAudioProcessor>();
        processor->prepareToPlay(48000.0, 256);
        set(*processor, "character", static_cast<float>(character));
        const juce::String name = character == 1 ? "purple" : (character == 0 ? "red" : "ice");
        for (float value : {0.0f, 0.5f, 1.0f})
        {
            for (const char* id : {"boom", "air", "bark", "space", "tide"}) set(*processor, id, value);
            std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
            save(*editor, folder.getChildFile(name + "-" + juce::String(static_cast<int>(value * 100)) + ".png"));
        }
    }
    auto processor = std::make_unique<PhoqerAudioProcessor>();
    processor->prepareToPlay(48000.0, 256);
    set(*processor, "space", 0.0f);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    juce::AudioBuffer<float> buffer(2, 256);
    float peak = 0.0f;
    for (int block = 0; block < 600; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.95f), 0);
        if (block == 80) midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        processor->processBlock(buffer, midi);
        peak = juce::jmax(peak, buffer.getMagnitude(0, buffer.getNumSamples()));
        refresh(*editor);
        if (block == 12 || block == 70 || block == 140 || block == 220 || block == 320)
            save(*editor, folder.getChildFile("real-call-" + juce::String(block) + ".png"));
    }
    processor->releaseResources();
    std::cout << "NO_WINDOWS_CREATED=1 THEMES=3 KNOB_POSITIONS=0,50,100 REAL_OUTPUT_PEAK=" << peak << '\n';
    return peak > 0.0f ? 0 : 1;
}
