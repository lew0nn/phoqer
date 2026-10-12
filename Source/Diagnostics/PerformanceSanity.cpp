#include "../Core/PhoqerEngine.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace phoqer;
namespace
{
void check(bool condition, const char* message) { if (! condition) throw std::runtime_error(message); }
void event(PhoqerEngine& engine, AudioBuffer& buffer, MacroState& macros, MidiEventType type, int note, float value, int channel = 0)
{
    MidiEvent e { type, 0, channel, note, value };
    engine.process(buffer, &e, 1, macros, 0);
}
void settle(PhoqerEngine& engine, AudioBuffer& buffer, const MacroState& macros, int blocks = 220)
{
    for (int i = 0; i < blocks; ++i) engine.process(buffer, nullptr, 0, macros, 0);
}
}

int main()
{
    try
    {
        {
            PhoqerEngine engine;
            AudioBuffer buffer(2, 128);
            MacroState macros;
            macros.space = 0;
            engine.prepare(48000, 128, 2);
            // No glide: a note over a held one starts on its own pitch, and chords still play.
            event(engine, buffer, macros, MidiEventType::noteOn, 48, 1);
            event(engine, buffer, macros, MidiEventType::noteOn, 60, 1);
            check(engine.getActiveVoiceCount() == 2, "poly no longer supports chords");
            check(engine.getNewestPitchNote() == 60, "glide off still slid");
            event(engine, buffer, macros, MidiEventType::allNotesOff, 0, 0);
            settle(engine, buffer, macros);
            check(engine.getActiveVoiceCount() == 0, "all notes off failed");

            // GLIDE 100 ms: the new note slides in from the held note, and the held note keeps sounding.
            macros.glideMs = 100;
            // A chord (struck together) does not glide, even with GLIDE up.
            {
                MidiEvent chord[] { { MidiEventType::noteOn, 0, 0, 52, 1 }, { MidiEventType::noteOn, 0, 0, 64, 1 } };
                engine.process(buffer, chord, 2, macros, 0);
                check(engine.getActiveVoiceCount() == 2 && engine.getNewestPitchNote() == 64, "a chord glided");
                event(engine, buffer, macros, MidiEventType::allNotesOff, 0, 0);
                settle(engine, buffer, macros);
            }
            event(engine, buffer, macros, MidiEventType::noteOn, 48, 1);
            settle(engine, buffer, macros, 12);         // 32 ms later: a note played over a held one
            event(engine, buffer, macros, MidiEventType::noteOn, 60, 1);
            check(engine.getActiveVoiceCount() == 2, "glide stole the held note");
            check(engine.getNewestPitchNote() > 48 && engine.getNewestPitchNote() < 49, "glide jumped to destination");
            settle(engine, buffer, macros, 18);
            check(engine.getNewestPitchNote() > 53 && engine.getNewestPitchNote() < 55, "glide timing is wrong");
            settle(engine, buffer, macros, 20);
            check(std::abs(engine.getNewestPitchNote() - 60) < 0.001f, "glide did not land");

            // Released first: a separate note does not glide.
            event(engine, buffer, macros, MidiEventType::allNotesOff, 0, 0);
            settle(engine, buffer, macros);
            event(engine, buffer, macros, MidiEventType::noteOn, 72, 1);
            check(engine.getNewestPitchNote() == 72, "a note after release glided");
        }

        uint64_t age = 0;
        SealVoice voice(123, age);
        MacroState macros;
        voice.prepare(48000, 128);
        voice.setMacros(macros);
        voice.startNote(0, 60, 1, 8192);
        check(voice.getPitchBend() == 0, "pitch wheel center is not exactly zero");
        AudioBuffer buffer(2, 512);
        voice.pitchWheelMoved(16383);
        voice.renderNextBlock(buffer, 0, 512);
        check(std::abs(voice.getPitchBend() - 2) < 0.001f, "positive bend failed");
        voice.pitchWheelMoved(0);
        voice.renderNextBlock(buffer, 0, 512);
        check(std::abs(voice.getPitchBend() + 2) < 0.001f, "negative bend failed");
        LinearSmoother smoother;
        smoother.reset(1000, 0.1);
        smoother.setCurrentAndTargetValue(0);
        for (int i = 0; i < 100; ++i) { smoother.setTargetValue(1); smoother.getNextValue(); }
        check(smoother.getCurrentValue() == 1, "repeated block targets restart smoothing");
        for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
            for (int size : { 32, 64, 128, 512, 1024 })
            {
                LookaheadLimiter limiter;
                limiter.prepare(rate);
                AudioBuffer loud(2, size);
                for (int block = 0; block < 80; ++block)
                {
                    for (int i = 0; i < size; ++i)
                    {
                        const int index = block * size + i;
                        const float v = index % 197 == 0 ? 20.0f : index % 37 < 12 ? 2.0f : -0.5f;
                        loud.setSample(0, i, v);
                        loud.setSample(1, i, -v * 0.7f);
                    }
                    limiter.process(loud);
                    check(loud.getMagnitude(0, size) <= 0.966101f, "limiter overshot on a transient/block boundary");
                    for (int i = 0; i < size; ++i)
                        check(std::abs(loud.getSample(1, i) + loud.getSample(0, i) * 0.7f) < 0.00001f,
                              "limiter did not link stereo gain");
                }
                limiter.reset();
                AudioBuffer impulse(2, limiter.getLatencySamples() + 2);
                impulse.setSample(0, 0, 0.5f);
                limiter.process(impulse);
                for (int i = 0; i < impulse.getNumSamples(); ++i)
                    check(std::abs(impulse.getSample(0, i) - (i == limiter.getLatencySamples() ? 0.5f : 0.0f)) < 0.00001f,
                          "limiter impulse latency differs from reported latency");
            }
        std::cout << "PERFORMANCE_SANITY_OK: poly glide, chords, bend, smoothing, limiter\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
