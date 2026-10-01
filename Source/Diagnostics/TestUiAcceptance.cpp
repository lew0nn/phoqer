// Offline renderer for the experimental PHOQER Test UI editor. Plays a real call through the
// engine for each character, ticks the editor like its 30 Hz timer would, and saves snapshots.

#include "../PluginProcessor.h"
#include "../TestUI/TestUiEditor.h"

#include <iostream>

namespace
{
void save(juce::Component& component, const juce::File& file)
{
    const auto image = component.createComponentSnapshot(component.getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream stream(file);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream(image, stream))
        throw std::runtime_error("Cannot save " + file.getFullPathName().toStdString());
}

void setParameter(PhoqerAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getParameters().getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc != 2) { std::cerr << "usage: PHOQERTestUIAcceptance <output-folder>\n"; return 2; }
    const juce::File folder(juce::String::fromUTF8(argv[1]));
    folder.createDirectory();

    constexpr double sampleRate = 48000.0;
    constexpr int block = 256;
    const int blocksPerFrame = juce::roundToInt(sampleRate / block / 30.0);    // ~30 Hz UI ticks
    const char* names[] { "burp", "squeal", "groan" };
    const int modes[] { 2, 0, 3 };                                             // BARK, CALL, WAIL

    for (int character : { 1, 0, 2 })
    {
        PhoqerAudioProcessor processor;
        processor.prepareToPlay(sampleRate, block);
        setParameter(processor, "character", static_cast<float>(character));
        setParameter(processor, "behavior", static_cast<float>(modes[character]));
        std::unique_ptr<juce::AudioProcessorEditor> base(processor.createEditor());
        auto* editor = dynamic_cast<phoqer::testui::TestUiEditor*>(base.get());
        if (editor == nullptr) { std::cerr << "editor is not the test UI\n"; return 1; }

        double now = 1000.0;
        save(*editor, folder.getChildFile(juce::String("testui-") + names[character] + "-idle.png"));
        juce::AudioBuffer<float> buffer(2, block);
        float maxIntensity = 0.0f;
        for (int b = 0; b < 260; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
            if (b == 90) midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            processor.processBlock(buffer, midi);
            maxIntensity = juce::jmax(maxIntensity, processor.getTelemetry().readFace().intensity);
            if (b % blocksPerFrame == 0)
            {
                now += 1.0 / 30.0;
                editor->tick(now);
            }
            if (b == 12 || b == 60 || b == 150)
                save(*editor, folder.getChildFile(juce::String("testui-") + names[character] + "-" + juce::String(b) + ".png"));
        }
        // MANIC, forced for the snapshot (live it flashes on hard BARK hits).
        editor->forceManicForTest(now);
        for (int k = 0; k < 4; ++k) { now += 1.0 / 30.0; editor->tick(now); }
        save(*editor, folder.getChildFile(juce::String("testui-") + names[character] + "-manic.png"));
        std::cout << names[character] << " max face intensity " << maxIntensity << '\n';
        base.reset();
        processor.releaseResources();
    }
    std::cout << "TEST_UI_ACCEPTANCE_OK\n";
    return 0;
}
