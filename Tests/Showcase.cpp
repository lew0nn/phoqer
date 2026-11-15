// Headless KYRAX showcase: plays a score through the real plugin with a fake DAW playhead (so ARP and YAP lock to the
// beat) and writes a WAV; optionally one snapshot of the real window per video frame.
// Usage: PHOQERShowcase <KYRAX.phqvoice> <score.txt> <out.wav> [frames folder]
// Score lines: "tempo 110", "length 64" (beats), "p <beat> <parameter id> <value>", "n <beat> <beats> <key> <velocity 0-1>",
// "lane <step 0-15> <velocity> <gate> <rest 0|1>". Anything else is ignored.
#include "PluginProcessor.h"
#include "UI/Editor.h"
#include <iostream>
#include <vector>

namespace {
constexpr double rate = 48000;
constexpr int block = 240, fps = 30;          // 200 blocks a second, so every video frame lands on a block

struct Head final : juce::AudioPlayHead {
    double beats = 0, bpm = 120;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm(bpm); p.setPpqPosition(beats); p.setIsPlaying(true);
        p.setTimeInSamples(static_cast<juce::int64>(beats * 60 / bpm * rate)); p.setTimeSignature(juce::AudioPlayHead::TimeSignature { 4, 4 });
        p.setPpqPositionOfLastBarStart(std::floor(beats / 4) * 4); return p;
    }
};
struct Note { double at, length; int key; float velocity; };
struct Change { double at; juce::String id; float value; };

void set(PhoqerAudioProcessor& p, const juce::String& id, float value)
{
    auto* v = p.getParameters().getParameter(id);
    if (v == nullptr) throw std::runtime_error(("no parameter " + id).toStdString());
    v->setValueNotifyingHost(v->convertTo0to1(value));
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 4) { std::cerr << "usage: PHOQERShowcase <voice> <score> <out.wav> [frames]\n"; return 2; }
    const juce::File voice { juce::String(argv[1]) }, scoreFile { juce::String(argv[2]) }, outFile { juce::String(argv[3]) };
    const juce::File frames = argc > 4 ? juce::File { juce::String(argv[4]) } : juce::File();
    try
    {
        PhoqerAudioProcessor p; juce::String error;
        if (!p.getVoices().loadVoiceFile(voice, error)) throw std::runtime_error(("voice: " + error).toStdString());
        p.getVoices().useLoadedVoice();
        Head head; double length = 64; std::vector<Note> notes; std::vector<Change> changes;
        juce::StringArray lines; scoreFile.readLines(lines);
        for (auto& line : lines)
        {
            auto t = juce::StringArray::fromTokens(line.trim(), " \t", ""); t.removeEmptyStrings();
            if (t.isEmpty()) continue;
            if (t[0] == "tempo") head.bpm = t[1].getDoubleValue();
            else if (t[0] == "length") length = t[1].getDoubleValue();
            else if (t[0] == "p") changes.push_back({ t[1].getDoubleValue(), t[2], t[3].getFloatValue() });
            else if (t[0] == "n") notes.push_back({ t[1].getDoubleValue(), t[2].getDoubleValue(), t[3].getIntValue(), t[4].getFloatValue() });
            else if (t[0] == "lane") p.setArpLane(t[1].getIntValue(), { t[2].getFloatValue(), t[3].getFloatValue(), t[4].getIntValue() != 0 });
        }
        std::stable_sort(changes.begin(), changes.end(), [](const Change& a, const Change& b) { return a.at < b.at; });
        p.setPlayHead(&head); p.prepareToPlay(rate, block);
        std::unique_ptr<juce::AudioProcessorEditor> base; phoqer::ui::PhoqerEditor* editor = nullptr;
        if (frames != juce::File())
        {
            frames.createDirectory(); base.reset(p.createEditor()); editor = dynamic_cast<phoqer::ui::PhoqerEditor*>(base.get());
            if (editor == nullptr) throw std::runtime_error("editor type");
            phoqer::ui::WindowControls controls; controls.audioSettings = controls.minimise = controls.toggleFullscreen = controls.close = [] {};
            editor->setWindowControls(controls);
        }
        const double bpm = head.bpm; const auto toSample = [bpm](double beat) { return static_cast<juce::int64>(std::llround(beat * 60 / bpm * rate)); };
        const int total = static_cast<int>(toSample(length)) / block * block;
        juce::AudioBuffer<float> mix(2, total), buffer(2, block); size_t nextChange = 0; int frame = 0;
        for (int start = 0; start + block <= total; start += block)
        {
            head.beats = start / rate * bpm / 60;
            while (nextChange < changes.size() && toSample(changes[nextChange].at) < start + block) { set(p, changes[nextChange].id, changes[nextChange].value); ++nextChange; }
            juce::MidiBuffer midi;
            for (const auto& n : notes)
            {
                const auto on = toSample(n.at), off = toSample(n.at + n.length);
                if (off >= start && off < start + block) midi.addEvent(juce::MidiMessage::noteOff(1, n.key), static_cast<int>(off - start));
                if (on >= start && on < start + block) midi.addEvent(juce::MidiMessage::noteOn(1, n.key, n.velocity), static_cast<int>(on - start));
            }
            buffer.clear(); p.processBlock(buffer, midi);
            for (int c = 0; c < 2; ++c) mix.copyFrom(c, start, buffer, c, 0, block);
            if (editor != nullptr && start + block >= static_cast<int>(std::llround(frame * rate / fps)))
            {
                editor->tick(start / rate);
                auto image = editor->getView().createComponentSnapshot(editor->getView().getLocalBounds(), true, 2);
                juce::FileOutputStream o(frames.getChildFile(juce::String::formatted("f%05d.jpg", frame)));
                juce::JPEGImageFormat jpg; jpg.setQuality(.93f); jpg.writeImageToStream(image, o); ++frame;
            }
        }
        p.releaseResources();
        outFile.deleteFile(); juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(outFile.createOutputStream().release(), rate, 2, 24, {}, 0));
        if (w == nullptr || !w->writeFromAudioSampleBuffer(mix, 0, total)) throw std::runtime_error("cannot write the WAV");
        std::cout << "SHOWCASE_OK " << total / rate << " s, peak " << mix.getMagnitude(0, total) << ", frames " << frame << '\n';
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "SHOWCASE FAILED: " << e.what() << '\n'; return 1; }
}
