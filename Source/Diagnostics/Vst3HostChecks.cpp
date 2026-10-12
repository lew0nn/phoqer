#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void check(bool condition, const char* message) { if (! condition) throw std::runtime_error(message); }
juce::AudioProcessorParameter& parameter(juce::AudioPluginInstance& plugin, const juce::String& name)
{
    for (auto* p : plugin.getParameters()) if (p->getName(100) == name) return *p;
    throw std::runtime_error("missing parameter " + name.toStdString());
}
void set(juce::AudioPluginInstance& p, const juce::String& name, float value)
{
    auto& param = parameter(p, name);
    param.beginChangeGesture(); param.setValueNotifyingHost(value); param.endChangeGesture();
}
struct Head final : juce::AudioPlayHead
{
    double bpm = 120;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info; info.setBpm(bpm); info.setIsPlaying(true); return info;
    }
};
struct Factory
{
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    explicit Factory(const juce::String& path)
    {
        format.findAllTypesForFile(descriptions, path);
        check(descriptions.size() == 1 && descriptions[0]->isInstrument, "VST3 scan failed");
    }
    std::unique_ptr<juce::AudioPluginInstance> make(double rate = 48000, int block = 512)
    {
        juce::String error;
        auto p = format.createInstanceFromDescription(*descriptions[0], rate, block, error);
        if (! p) throw std::runtime_error(error.toStdString());
        p->setRateAndBufferSizeDetails(rate, block);
        p->prepareToPlay(rate, block);
        return p;
    }
};
float render(juce::AudioPluginInstance& p, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear(); p.processBlock(buffer, midi);
    float peak = 0;
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float v = buffer.getSample(c, i);
            check(std::isfinite(v) && std::abs(v) <= 0.967f, "VST3 output invalid or above ceiling");
            peak = std::max(peak, std::abs(v));
        }
    return peak;
}

void recallAndInstances(Factory& factory)
{
    auto a = factory.make(), b = factory.make();
    Head head; a->setPlayHead(&head); b->setPlayHead(&head);
    set(*a, "BOOM", 0.9f); set(*a, "TIDE", 0.2f); set(*a, "BEHAVIOR", 0.5f);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    render(*a, buffer, midi);
    check(std::abs(parameter(*b, "BOOM").getValue() - 0.5f) < 0.001f, "instances shared parameters");
    juce::MemoryBlock state;
    a->getStateInformation(state);
    b->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    render(*b, buffer, midi);
    for (const char* name : { "BOOM", "TIDE", "BEHAVIOR" })
        check(std::abs(parameter(*a, name).getValue() - parameter(*b, name).getValue()) < 0.001f, "VST3 session recall failed");
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    render(*a, buffer, midi);
    midi.clear();
    check(render(*b, buffer, midi) == 0, "instances shared voice state");

    // Editor recreation exercises the actual binary's editor and VST3 view adapter.
    for (int i = 0; i < 3; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(a->createEditorIfNeeded());
        check(editor != nullptr && editor->getWidth() > 0 && editor->getHeight() > 0, "VST3 editor failed");
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        check(image.isValid(), "VST3 editor snapshot failed");
    }
    a->setPlayHead(nullptr); b->setPlayHead(nullptr);
    a->releaseResources(); b->releaseResources();
    std::cout << "VST3_RECALL_INSTANCES_EDITOR_OK\n";
}

void tempo(Factory& factory)
{
    auto a = factory.make(), b = factory.make();
    Head slow, fast; slow.bpm = fast.bpm = 90;
    a->setPlayHead(&slow); b->setPlayHead(&fast);
    set(*a, "BEHAVIOR", 0.25f); set(*b, "BEHAVIOR", 0.25f);
    juce::AudioBuffer<float> x(2, 512), y(2, 512);
    double equalDifference = 0, tempoDifference = 0;
    for (int block = 0; block < 90; ++block)
    {
        juce::MidiBuffer ma, mb;
        if (block == 0)
        {
            ma.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
            mb.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
        }
        if (block == 45) fast.bpm = 180;
        render(*a, x, ma); render(*b, y, mb);
        for (int i = 0; i < 512; ++i)
        {
            auto& destination = block < 45 ? equalDifference : tempoDifference;
            destination += std::abs(x.getSample(0, i) - y.getSample(0, i));
        }
    }
    check(equalDifference < 0.001, "two instances at the same host tempo differ");
    check(tempoDifference > 0.01, "VST3 did not follow host tempo change");
    a->setPlayHead(nullptr); b->setPlayHead(nullptr);
    a->releaseResources(); b->releaseResources();
    std::cout << "VST3_HOST_TEMPO_OK\n";
}

void matrix(Factory& factory, const juce::File& report)
{
    juce::Array<juce::var> rows;
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (int block : { 32, 64, 128, 512, 1024 })
        {
            auto p = factory.make(rate, block);
            Head head; p->setPlayHead(&head);
            check(p->getLatencySamples() == std::max(8, static_cast<int>(0.002 * rate)), "VST3 latency reporting failed");
            double elapsed = 0, audioSeconds = 0;
            float peak = 0;
            for (int character = 0; character < 3; ++character)
                for (int mode = 0; mode < 5; ++mode)
                {
                    p->releaseResources(); p->prepareToPlay(rate, block);
                    set(*p, "CHARACTER", character * 0.5f);
                    set(*p, "BEHAVIOR", mode * 0.25f);
                    juce::AudioBuffer<float> buffer(2, block);
                    const int blocks = static_cast<int>(std::ceil(rate * 0.35 / block));
                    float casePeak = 0;
                    for (int b = 0; b < blocks; ++b)
                    {
                        juce::MidiBuffer midi;
                        if (b == 0)
                            for (int note : { 36, 43, 48, 52, 55, 60, 64, 67 })
                                midi.addEvent(juce::MidiMessage::noteOn(1, note, 1.0f), 0);
                        if (b == blocks - 1) midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
                        // Host-style automation at block boundaries, including extreme values.
                        const float automation = static_cast<float>(b % 17) / 16;
                        set(*p, "BOOM", automation); set(*p, "VOWEL", 1 - automation);
                        set(*p, "AIR", automation); set(*p, "TIDE", automation);
                        set(*p, "REVERB", automation); set(*p, "DETUNE", automation);
                        const auto start = juce::Time::getMillisecondCounterHiRes();
                        const float value = render(*p, buffer, midi);
                        elapsed += (juce::Time::getMillisecondCounterHiRes() - start) * 0.001;
                        audioSeconds += block / rate;
                        casePeak = std::max(casePeak, value);
                    }
                    check(casePeak > 0.00001f, "silent VST3 character/mode combination");
                    peak = std::max(peak, casePeak);
                }
            auto* row = new juce::DynamicObject;
            row->setProperty("sampleRate", rate); row->setProperty("blockSize", block);
            row->setProperty("cases", 15); row->setProperty("peak", peak);
            row->setProperty("offlineProcessPercentOfAudioTime", 100 * elapsed / audioSeconds);
            rows.add(juce::var(row));
            std::cout << "rate=" << rate << " block=" << block << " cases=15 CPU=" << 100 * elapsed / audioSeconds << "%\n";
            p->setPlayHead(nullptr); p->releaseResources();
        }
    check(report.replaceWithText(juce::JSON::toString(juce::var(rows))), "cannot save host report");
    std::cout << "VST3_MATRIX_OK: 300 character/mode/rate/block cases\n";
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        if (argc != 3) { std::cerr << "usage: PHOQERVst3HostChecks <bundle> <report.json>\n"; return 2; }
        Factory factory(juce::String::fromUTF8(argv[1]));
        recallAndInstances(factory); tempo(factory);
        matrix(factory, juce::File(juce::String::fromUTF8(argv[2])));
        std::cout << "VST3_HOST_CHECKS_OK\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
