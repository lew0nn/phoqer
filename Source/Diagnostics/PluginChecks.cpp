#include "../PluginProcessor.h"
#include "../UI/Editor.h"
#include "../UI/Presets.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool condition, const char* message) { if (! condition) throw std::runtime_error(message); }
void set(PhoqerAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.getParameters().getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(PhoqerAudioProcessor& p, const char* id) { return p.getParameters().getRawParameterValue(id)->load(); }
void stateAndControls()
{
    auto owner = std::make_unique<PhoqerAudioProcessor>();
    auto& p = *owner;
    phoqer::ui::PresetLibrary presets(p.getParameters());
    presets.load(2);
    set(p, "output", -7);
    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    auto restoredOwner = std::make_unique<PhoqerAudioProcessor>();
    auto& restored = *restoredOwner;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (auto* parameter : p.juce::AudioProcessor::getParameters())
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        check(param != nullptr, "unexpected parameter type");
        auto* other = restored.getParameters().getParameter(param->paramID);
        check(other && std::abs(other->getValue() - param->getValue()) < 0.0001f, "session parameter recall failed");
    }
    // The real view opens, reopens, and has no leftover comparison controls.
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto& view = dynamic_cast<phoqer::ui::PhoqerEditor&>(*editor).getView();
    check(view.findChildWithID("comparisonA") == nullptr && view.findChildWithID("comparisonB") == nullptr
          && view.findChildWithID("comparisonCopy") == nullptr, "comparison controls remain");
    editor.reset();
    editor.reset(p.createEditor());
    editor.reset();

    // A preset file sets the sound only: an OUTPUT inside it is ignored.
    juce::TemporaryFile oldPreset(".phqpreset");
    check(oldPreset.getFile().replaceWithText("<PHOQERPRESET name=\"LEGACY\"><PARAM id=\"boom\" value=\"0.3\"/>"
                                              "<PARAM id=\"output\" value=\"12\"/></PHOQERPRESET>"),
          "cannot create legacy preset fixture");
    const float level = get(p, "output");
    check(presets.loadFile(oldPreset.getFile()), "legacy preset did not load");
    check(std::abs(get(p, "boom") - 0.3f) < 0.001f, "preset sound value not applied");
    check(get(p, "output") == level, "preset changed output");
    p.prepareToPlay(48000, 128);
    set(p, "space", 0); set(p, "detune", 0);
    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer burst;
    burst.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    p.processBlock(buffer, burst);
    burst.clear();
    for (int i = 0; i < 400; ++i) burst.addEvent(juce::MidiMessage::noteOn(1, 48 + i % 24, 1.0f), i % 128);
    burst.addEvent(juce::MidiMessage::allNotesOff(1), 127);
    p.processBlock(buffer, burst);
    burst.clear();
    for (int i = 0; i < 1200; ++i) p.processBlock(buffer, burst);
    check(buffer.getMagnitude(0, 128) < 0.00001f, "MIDI overflow left stuck notes");
    burst.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 120);
    p.panic(); p.processBlock(buffer, burst); burst.clear();
    for (int i = 0; i < 1200; ++i) p.processBlock(buffer, burst);
    check(buffer.getMagnitude(0, 128) < 0.00001f, "panic failed with late-block notes");
    p.releaseResources();
    std::cout << "PLUGIN_STATE_CONTROLS_OK\n";
}

struct Metrics
{
    double peak = 0, square = 0, sum = 0, high = 0, spectral = 0;
    int samples = 0;
    juce::dsp::FFT fft { 11 };
    std::array<float, 4096> work {};
    int fill = 0;
    void add(const juce::AudioBuffer<float>& buffer)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            for (int c = 0; c < buffer.getNumChannels(); ++c)
            {
                const auto v = buffer.getSample(c, i);
                check(std::isfinite(v) && std::abs(v) <= 0.967f, "preset exceeded limiter ceiling or became nonfinite");
                peak = std::max(peak, static_cast<double>(std::abs(v)));
                square += v * v; sum += v; ++samples;
            }
            work[static_cast<size_t>(fill++)] = (buffer.getSample(0, i) + buffer.getSample(1, i)) * 0.5f;
            if (fill == 2048)
            {
                for (int n = 0; n < 2048; ++n)
                    work[static_cast<size_t>(n)] *= 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * n / 2047.0f);
                fft.performFrequencyOnlyForwardTransform(work.data());
                for (int bin = 1; bin < 1024; ++bin)
                {
                    const auto energy = static_cast<double>(work[static_cast<size_t>(bin)]) * work[static_cast<size_t>(bin)];
                    spectral += energy;
                    if (bin * 48000.0 / 2048 >= 6000) high += energy;
                }
                work.fill(0); fill = 0;
            }
        }
    }
};

void audit(const juce::File& folder)
{
    folder.createDirectory();
    juce::Array<juce::var> results;
    auto bankOwner = std::make_unique<PhoqerAudioProcessor>();
    auto& bank = *bankOwner;
    phoqer::ui::PresetLibrary library(bank.getParameters());
    constexpr int block = 256;
    for (int preset = 0; preset < 16; ++preset)
    {
        juce::AudioBuffer<float> demo(2, 48000 * 9);
        demo.clear();
        for (int scenario = 0; scenario < 10; ++scenario)
        {
            auto owner = std::make_unique<PhoqerAudioProcessor>();
            auto& p = *owner;
            phoqer::ui::PresetLibrary presets(p.getParameters());
            presets.load(preset);
            p.prepareToPlay(48000, block);
            const bool chord = scenario == 9;
            const int note = chord ? 48 : 36 + (scenario / 3) * 24;
            const float velocity = chord ? 1.0f : (scenario % 3 == 0 ? 0.25f : scenario % 3 == 1 ? 0.6f : 1.0f);
            Metrics metrics;
            juce::AudioBuffer<float> buffer(2, block);
            for (int b = 0; b < 375; ++b)
            {
                juce::MidiBuffer midi;
                if (b == 0)
                    for (int offset : { 0, 4, 7, 12 })
                    {
                        midi.addEvent(juce::MidiMessage::noteOn(1, note + offset, velocity), 0);
                        if (! chord) break;
                    }
                if (b == 281)
                    for (int offset : { 0, 4, 7, 12 })
                    {
                        midi.addEvent(juce::MidiMessage::noteOff(1, note + offset), 0);
                        if (! chord) break;
                    }
                p.processBlock(buffer, midi);
                metrics.add(buffer);
                // Fixed level examples: low, middle, high at full velocity, then a chord.
                if (scenario == 2 || scenario == 5 || scenario == 8 || scenario == 9)
                {
                    const int start = (scenario == 9 ? 3 : scenario / 3) * 108000 + b * block;
                    for (int c = 0; c < 2; ++c) demo.copyFrom(c, start, buffer, c, 0, block);
                }
            }
            check(metrics.peak > 0.0001, "silent factory preset");
            auto* row = new juce::DynamicObject;
            row->setProperty("preset", library.all()[static_cast<size_t>(preset)].name);
            row->setProperty("note", chord ? juce::var("48,52,55,60") : juce::var(note));
            row->setProperty("velocity", velocity);
            row->setProperty("peakDbFS", 20 * std::log10(metrics.peak));
            row->setProperty("rmsDbFS", 10 * std::log10(std::max(1.0e-15, metrics.square / metrics.samples)));
            row->setProperty("dc", metrics.sum / metrics.samples);
            row->setProperty("energyAbove6kHzPercent", metrics.spectral > 0 ? 100 * metrics.high / metrics.spectral : 0);
            results.add(juce::var(row));
        }
        auto name = library.all()[static_cast<size_t>(preset)].name.replaceCharacter(' ', '-');
        juce::WavAudioFormat wav;
        auto stream = folder.getChildFile(name + ".wav").createOutputStream();
        check(stream != nullptr, "cannot create demo WAV");
        stream->setPosition(0); stream->truncate();
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(), 48000, 2, 24, {}, 0));
        check(writer != nullptr && writer->writeFromAudioSampleBuffer(demo, 0, demo.getNumSamples()), "cannot write demo WAV");
        std::cout << "audited " << name << '\n';
    }
    check(folder.getChildFile("preset-audit.json").replaceWithText(juce::JSON::toString(juce::var(results))), "cannot write preset audit");
    std::cout << "FACTORY_PRESET_AUDIT_OK: 160 cases, 16 listening examples\n";
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        stateAndControls();
        if (argc == 2) audit(juce::File(juce::String::fromUTF8(argv[1])));
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
