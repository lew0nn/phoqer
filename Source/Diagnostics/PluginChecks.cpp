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
    set(p, "output", -7); set(p, "standaloneTempo", 173);
    set(p, "metronomeEnabled", 1);
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
    // Exercise the editable tempo field and its parameter history in the real view.
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto& view = dynamic_cast<phoqer::ui::PhoqerEditor&>(*editor).getView();
    auto* metro = view.findChildWithID("metroWindow");
    check(metro != nullptr, "missing METRO.EXE window");
    auto* tempo = metro->findChildWithID("tempo");
    check(tempo != nullptr, "missing tempo field");
    auto* label = dynamic_cast<juce::Label*>(tempo->getChildComponent(0));
    check(label != nullptr, "tempo field is not editable");
    label->setText("173", juce::sendNotificationSync);
    check(get(p, "standaloneTempo") == 173, "typed tempo did not reach processor");
    label->setText("999", juce::sendNotificationSync);
    check(get(p, "standaloneTempo") == 400, "tempo upper bound failed");
    label->setText("1", juce::sendNotificationSync);
    check(get(p, "standaloneTempo") == 20, "tempo lower bound failed");
    check(label->isEditableOnSingleClick(), "tempo requires more than one click");
    view.keyPressed(juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0));
    check(get(p, "standaloneTempo") == 400, "tempo undo failed");
    view.keyPressed(juce::KeyPress('y', juce::ModifierKeys::ctrlModifier, 0));
    check(get(p, "standaloneTempo") == 20, "tempo redo failed");
    check(view.findChildWithID("comparisonA") == nullptr && view.findChildWithID("comparisonB") == nullptr
          && view.findChildWithID("comparisonCopy") == nullptr, "comparison controls remain");
    set(p, "metronomeEnabled", 0);
    phoqer::ui::WindowControls preview;
    preview.audioSettings = [] {};
    view.setWindowControls(preview);
    check(tempo->isVisible(), "standalone tempo hidden");
    auto* opener = dynamic_cast<juce::Button*>(view.findChildWithID("metroOpen"));
    check(opener != nullptr && opener->isVisible() && ! metro->isVisible(), "METRO.EXE should start closed with the KEYS.EXE BPM shown");
    auto* barSwitch = dynamic_cast<juce::Button*>(view.findChildWithID("barMetronome"));
    check(barSwitch != nullptr && barSwitch->isVisible() && ! barSwitch->getBounds().intersects(opener->getBounds()),
          "missing or overlapping KEYS.EXE metronome switch");
    opener->onClick();
    check(metro->isVisible() && view.getLocalBounds().contains(metro->getBounds()), "METRO.EXE did not open inside the editor");
    auto* click = dynamic_cast<juce::Button*>(metro->findChildWithID("metronome"));
    check(click != nullptr && click->isVisible(), "missing standalone metronome toggle");
    check(click->getClickingTogglesState(), "metronome button does not toggle on click");
    check(! click->getBounds().intersects(tempo->getBounds()), "metronome overlaps tempo");
    click->setToggleState(true, juce::sendNotificationSync);
    view.tick(999.0);
    check(get(p, "metronomeEnabled") == 1, "click toggle did not enable metronome");
    view.keyPressed(juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0));
    check(get(p, "metronomeEnabled") == 0, "metronome toggle undo failed");
    editor.reset();
    editor.reset(p.createEditor());
    check(get(p, "standaloneTempo") == 20, "editor reopening reset tempo");
    editor.reset();

    for (const auto* id : { "standaloneTempo", "metronomeEnabled", "metronomeVolume", "metronomeBeats", "metronomeAccent", "metronomeSound", "countIn" })
        check(! p.getParameters().getParameter(id)->isAutomatable(), "a standalone-only setting is automatable in a DAW");
    // Strip the new parameters to simulate a pre-feature session loaded over edited state.
    auto oldState = p.getParameters().copyState();
    for (const auto* id : { "standaloneTempo", "metronomeEnabled" })
        oldState.removeChild(oldState.getChildWithProperty("id", id), nullptr);
    auto xml = oldState.createXml();
    juce::MemoryBlock legacy;
    juce::AudioProcessor::copyXmlToBinary(*xml, legacy);
    p.setStateInformation(legacy.getData(), static_cast<int>(legacy.getSize()));
    check(get(p, "standaloneTempo") == 120 && get(p, "metronomeEnabled") == 0, "legacy session defaults failed");
    juce::TemporaryFile oldPreset(".phqpreset");
    check(oldPreset.getFile().replaceWithText("<PHOQERPRESET name=\"LEGACY\"><PARAM id=\"boom\" value=\"0.3\"/><PARAM id=\"output\" value=\"12\"/></PHOQERPRESET>"),
          "cannot create legacy preset fixture");
    const float level = get(p, "output");
    set(p, "metronomeEnabled", 1);
    check(presets.loadFile(oldPreset.getFile()), "legacy preset did not load");
    check(get(p, "output") == level && get(p, "standaloneTempo") == 120, "preset changed output or tempo");
    check(get(p, "metronomeEnabled") == 1, "preset changed metronome toggle");
    set(p, "metronomeEnabled", 0);
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

void metronomeAudio()
{
    const phoqer::MetronomeSettings flat { 0.7f, 4, false, 0 };     // timing is measured on unaccented clicks
    // Verify sample-accurate beat intervals across supported rates, block sizes and tempo bounds.
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (int size : { 32, 64, 128, 512, 1024 })
            for (float bpm : { 20.0f, 120.0f, 400.0f })
            {
                phoqer::Metronome click;
                click.prepare(rate);
                phoqer::AudioBuffer buffer(2, size);
                const double interval = rate * 60.0 / bpm;
                const int total = static_cast<int>(std::ceil(interval * 2.2));
                std::vector<int> starts;
                int lastSound = -static_cast<int>(rate);
                for (int offset = 0; offset < total; offset += size)
                {
                    buffer.clear();
                    click.process(buffer, true, bpm, flat);
                    for (int i = 0; i < size; ++i)
                        if (std::abs(buffer.getSample(0, i)) > 0.001f)
                        {
                            const int sample = offset + i;
                            if (sample - lastSound > static_cast<int>(0.05 * rate)) starts.push_back(sample);
                            lastSound = sample;
                            check(buffer.getSample(0, i) == buffer.getSample(1, i), "metronome stereo mismatch");
                        }
                }
                check(starts.size() == 3, "incorrect number of metronome beats");
                for (size_t i = 1; i < starts.size(); ++i)
                    check(std::abs((starts[i] - starts[i - 1]) - interval) <= 2.0, "metronome beat timing drift");
                for (int i = 0; i < 1500 / size + 2; ++i) { buffer.clear(); click.process(buffer, false, bpm, flat); }
                check(buffer.getMagnitude(0, size) == 0, "disabled metronome kept clicking");
            }

    phoqer::Metronome click;
    click.prepare(48000);
    phoqer::AudioBuffer sample(2, 1);
    std::vector<int> starts;
    int lastSound = -48000;
    for (int i = 0; i < 36000; ++i)
    {
        sample.clear(); click.process(sample, true, i < 12000 ? 120.0f : 240.0f, flat);
        if (std::abs(sample.getSample(0, 0)) > 0.001f)
        {
            if (i - lastSound > 2400) starts.push_back(i);
            lastSound = i;
        }
    }
    check(starts.size() == 3 && std::abs((starts[1] - starts[0]) - 18000) <= 2
          && std::abs((starts[2] - starts[1]) - 12000) <= 2, "tempo change lost metronome beat phase");
    click.reset();
    for (int i = 0; i < 1000; ++i)
    {
        sample.setSample(0, 0, 0.9661f); sample.setSample(1, 0, -0.9661f);
        click.process(sample, true, 400, flat);
        check(sample.getMagnitude(0, 1) <= 0.96611f, "metronome mix exceeded output ceiling");
    }

    // Check the real processor routing: audible monitor click, silent no-note WAV take.
    auto p = std::make_unique<PhoqerAudioProcessor>();
    p->prepareToPlay(48000, 128);
    set(*p, "metronomeEnabled", 1);
    check(p->getRecorder().start(48000, 2), "cannot start metronome recorder check");
    juce::AudioBuffer<float> monitor(2, 128);
    juce::MidiBuffer midi;
    float peak = 0;
    for (int i = 0; i < 40; ++i) { p->processBlock(monitor, midi); peak = std::max(peak, monitor.getMagnitude(0, 128)); }
    check(peak > 0.05f, "metronome did not reach standalone output");
    const auto take = p->getRecorder().stop();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(take.createInputStream().release(), true));
    check(reader != nullptr && reader->lengthInSamples > 0, "metronome recorder check produced no take");
    juce::AudioBuffer<float> recorded(2, static_cast<int>(reader->lengthInSamples));
    check(reader->read(&recorded, 0, recorded.getNumSamples(), 0, true, true), "cannot read metronome recorder check");
    check(recorded.getMagnitude(0, recorded.getNumSamples()) == 0, "metronome leaked into WAV take");
    set(*p, "metronomeEnabled", 0);
    for (int i = 0; i < 10; ++i) p->processBlock(monitor, midi);
    check(monitor.getMagnitude(0, 128) == 0, "processor kept clicking after disable");
    p->releaseResources();
    // METRO.EXE settings: volume 0 is silent, every sound clicks, beat 1 is accented, the count-in is not recorded.
    {
        const auto peaks = [](phoqer::MetronomeSettings settings)
        {
            phoqer::Metronome m;
            m.prepare(48000);
            phoqer::AudioBuffer block(2, 12000);      // one beat at 240 BPM
            std::array<float, 2> result {};
            for (auto& r : result) { block.clear(); m.process(block, true, 240, settings); r = block.getMagnitude(0, 12000); }
            return result;
        };
        phoqer::MetronomeSettings quiet;
        quiet.volume = 0;
        check(peaks(quiet)[0] == 0, "metronome volume 0 still clicks");
        for (int sound = 0; sound < 3; ++sound)
        {
            phoqer::MetronomeSettings settings;
            settings.sound = sound;
            const auto beat = peaks(settings);
            check(beat[1] > 0.02f && beat[0] > beat[1] * 1.1f, "metronome sound silent or beat 1 not accented");
            settings.accent = false;
            const auto flat = peaks(settings);
            check(std::abs(flat[0] - flat[1]) < flat[1] * 0.05f, "accent off still accents");
        }
    }
    {
        auto q = std::make_unique<PhoqerAudioProcessor>();
        q->prepareToPlay(48000, 128);
        set(*q, "metronomeEnabled", 0);
        set(*q, "countIn", 1);                       // 1 bar of 4/4 at 120 BPM = 96000 samples
        check(q->getCountInBars() == 1 && q->getMetronomeBeatsPerBar() == 4, "count-in settings not read");
        check(q->getRecorder().start(48000, 2, 96000), "cannot start count-in recorder check");
        juce::AudioBuffer<float> block(2, 128);
        juce::MidiBuffer none;
        float countPeak = 0;
        for (int i = 0; i < 800; ++i) { q->processBlock(block, none); if (i < 20) countPeak = std::max(countPeak, block.getMagnitude(0, 128)); }
        check(countPeak > 0.05f, "count-in did not click with the metronome off");
        check(block.getMagnitude(0, 128) == 0, "click continued after the count-in with the metronome off");
        check(std::llround(q->getRecorder().seconds() * 48000) == 800 * 128 - 96000, "count-in was recorded or cut short");
        q->getRecorder().stop().deleteFile();
        q->releaseResources();
    }
    std::cout << "METRONOME_AUDIO_OK: timing matrix, tempo change, stop, ceiling, monitor and recording routing, "
                 "volume, sounds, accent, count-in\n";
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
        metronomeAudio();
        if (argc == 2) audit(juce::File(juce::String::fromUTF8(argv[1])));
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
