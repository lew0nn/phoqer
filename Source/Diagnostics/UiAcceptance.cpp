// Offline renderer for the PHOQER editor. Plays a real call through the
// engine for each character, ticks the editor like its 30 Hz timer would, and saves snapshots.

#include "../PluginProcessor.h"
#include "../UI/Editor.h"
#include "../UI/Logo.h"

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
    if (argc != 2) { std::cerr << "usage: PHOQERUIAcceptance <output-folder>\n"; return 2; }
    const juce::File folder(juce::String::fromUTF8(argv[1]));
    folder.createDirectory();

    if (juce::SystemStats::getEnvironmentVariable("PHOQER_SPLASH", {}).isNotEmpty())
    {
        const std::array<std::pair<const char*, float>, 4> frames {{
            { "startup-00.png", 0.0f },
            { "startup-33.png", 0.33f },
            { "startup-66.png", 0.66f },
            { "startup-100.png", 1.0f }
        }};
        for (const auto& [name, progress] : frames)
        {
            juce::Image image(juce::Image::RGB, phoqer::ui::bootSplashWidth * 2,
                              phoqer::ui::bootSplashHeight * 2, true, juce::SoftwareImageType());
            {
                juce::Graphics g(image);
                g.addTransform(juce::AffineTransform::scale(2.0f));
                phoqer::ui::paintStandaloneBootSplash(
                    g, { 0.0f, 0.0f, static_cast<float>(phoqer::ui::bootSplashWidth),
                         static_cast<float>(phoqer::ui::bootSplashHeight) }, progress);
            }
            juce::FileOutputStream output(folder.getChildFile(name));
            if (! output.openedOk() || ! juce::PNGImageFormat().writeImageToStream(image, output))
                throw std::runtime_error(juce::String("Cannot save ").toStdString() + name);
        }
        std::cout << "STANDALONE_SPLASH_OK\n";
        return 0;
    }

    constexpr double sampleRate = 48000.0;
    constexpr int block = 256;
    const int blocksPerFrame = juce::roundToInt(sampleRate / block / 30.0);    // ~30 Hz UI ticks
    const char* names[] { "burp", "squeal", "groan" };
    const int modes[] { 2, 0, 3 };                                             // BARK, CALL, WAIL

    for (int character : { 1, 0, 2 })
    {
        auto owner = std::make_unique<PhoqerAudioProcessor>();
        auto& processor = *owner;
        processor.prepareToPlay(sampleRate, block);
        setParameter(processor, "character", static_cast<float>(character));
        // PHOQER_UI_MODE=<0-4> plays every seal in the same mood (for matching README animations).
        const auto forcedMode = juce::SystemStats::getEnvironmentVariable("PHOQER_UI_MODE", {});
        setParameter(processor, "behavior", static_cast<float>(forcedMode.isNotEmpty() ? forcedMode.getIntValue() : modes[character]));
        std::unique_ptr<juce::AudioProcessorEditor> base(processor.createEditor());
        auto* editor = dynamic_cast<phoqer::ui::PhoqerEditor*>(base.get());
        if (editor == nullptr) { std::cerr << "editor is not the PHOQER editor\n"; return 1; }

        double now = 1000.0;
        save(editor->getView(), folder.getChildFile(juce::String("ui-") + names[character] + "-idle.png"));
        phoqer::ui::WindowControls preview;
        preview.audioSettings = preview.minimise = preview.toggleFullscreen = preview.close = [] {};
        editor->setWindowControls(preview);
        save(editor->getView(), folder.getChildFile(juce::String("ui-") + names[character] + "-standalone.png"));
        setParameter(processor, "metronomeEnabled", 1);
        editor->tick(now);
        save(editor->getView(), folder.getChildFile(juce::String("ui-") + names[character] + "-click-on.png"));
        // The metronome's swing and click flash: two beats at 120 BPM, 30 frames a second.
        // METRO.EXE open over the app, the click on.
        if (auto* opener = dynamic_cast<juce::Button*>(editor->getView().findChildWithID("metroOpen")))
        {
            opener->onClick();
            save(editor->getView(), folder.getChildFile(juce::String("ui-") + names[character] + "-metro.png"));
        }
        auto* metroWindow = editor->getView().findChildWithID("metroWindow");
        if (auto* metronome = metroWindow == nullptr ? nullptr : dynamic_cast<phoqer::ui::MetronomeButton98*>(metroWindow->findChildWithID("metronome")))
        {
            const auto previous = metronome->beats;
            float beat = 0.0f;
            metronome->beats = [&beat] { return beat; };
            for (int frame = 0; frame < 30; ++frame)
            {
                beat = frame / 15.0f;
                save(*metronome, folder.getChildFile(juce::String("swing-") + names[character] + "-"
                                                     + juce::String(frame).paddedLeft('0', 2) + ".png"));
            }
            metronome->beats = previous;
        }
        setParameter(processor, "metronomeEnabled", 0);
        editor->tick(now);
        editor->setWindowControls({});
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
                // PHOQER_FRAMES=1: every frame of the call, for the README's animated seals.
                if (juce::SystemStats::getEnvironmentVariable("PHOQER_FRAMES", {}).isNotEmpty())
                    save(editor->getView(), folder.getChildFile(juce::String("frame-") + names[character] + "-"
                                                                + juce::String(b / blocksPerFrame).paddedLeft('0', 3) + ".png"));
            }
            if (b == 12 || b == 60 || b == 150)
                save(editor->getView(), folder.getChildFile(juce::String("ui-") + names[character] + "-" + juce::String(b) + ".png"));
        }
        // MANIC, forced for the snapshot (live it flashes on hard BARK hits).
        editor->forceManicForTest(now);
        for (int k = 0; k < 4; ++k) { now += 1.0 / 30.0; editor->tick(now); }
        save(editor->getView(), folder.getChildFile(juce::String("ui-") + names[character] + "-manic.png"));
        std::cout << names[character] << " max face intensity " << maxIntensity << '\n';
        base.reset();
        processor.releaseResources();
    }
    std::cout << "TEST_UI_ACCEPTANCE_OK\n";
    return 0;
}
