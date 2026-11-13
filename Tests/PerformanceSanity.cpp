#include "Core/PhoqerEngine.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <vector>

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

// --cpu: the share of one core PHOQER needs at 44.1 kHz in 128-sample blocks, for 1, 4 and 8 held
// notes of each seal (20 s of audio each). This is the table in the README.
int cpuReport()
{
    constexpr double rate = 44100.0, seconds = 20.0;
    constexpr int block = 128;
    std::cout << "notes  BURP  SQUEAL  GROAN\n";
    for (int notes : { 1, 4, 8 })
    {
        std::cout << notes << "     ";
        for (int seal = 0; seal < 3; ++seal)
        {
            auto owned = std::make_unique<PhoqerEngine>();     // too big for the stack
            auto& engine = *owned;
            AudioBuffer buffer(2, block);
            MacroState macros;
            macros.character = static_cast<SealCharacter>(seal);
            engine.prepare(rate, block, 2);
            for (int n = 0; n < notes; ++n)
            {
                MidiEvent on { MidiEventType::noteOn, 0, 0, 48 + n * 3, 0.8f };
                engine.process(buffer, &on, 1, macros, 0);
            }
            const int blocks = static_cast<int>(seconds * rate / block);
            const auto start = std::chrono::steady_clock::now();
            for (int b = 0; b < blocks; ++b) engine.process(buffer, nullptr, 0, macros, 0);
            const double used = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::cout << "  " << static_cast<int>(used / seconds * 1000.0 + 0.5) / 10.0 << "%";
        }
        std::cout << "\n";
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--cpu") == 0) return cpuReport();
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

        for (const float bend : { 2.4f, 12.0f, 24.0f })     // WAIL BEND reaches the voice: the sweep peaks at the set amount
        {
            uint64_t wailAge = 0;
            SealVoice wail(7, wailAge);
            MacroState m;
            m.behaviorMode = BehaviourMode::wail;
            m.wailBend = bend;
            wail.prepare(48000, 512);
            wail.setMacros(m);
            wail.startNote(0, 60, 1, 8192);
            AudioBuffer block(2, 512);
            float peak = 0.0f;
            for (int i = 0; i < 48000 * 2 / 512; ++i) { wail.renderNextBlock(block, 0, 512); peak = std::max(peak, std::abs(wail.getModeSemitones())); }
            check(std::abs(peak - bend) < 0.05f * bend, "WAIL BEND did not reach the voice");
        }
        {   // WAIL HOLD: up to the bend in two beats (1 s at 120 BPM), then it stays there
            uint64_t holdAge = 0;
            SealVoice wail(9, holdAge);
            MacroState m;
            m.behaviorMode = BehaviourMode::wail;
            m.wailBend = 12.0f;
            m.wailHold = true;
            wail.prepare(48000, 480);
            wail.setMacros(m);
            wail.startNote(0, 60, 1, 8192);
            AudioBuffer block(2, 480);
            for (int i = 0; i < 400; ++i)     // 4 s: past where the swing would have come back down
            {
                wail.renderNextBlock(block, 0, 480);
                if (i == 50) check(wail.getModeSemitones() > 6.0f && wail.getModeSemitones() < 11.0f, "WAIL HOLD did not rise");
                if (i >= 101) check(std::abs(wail.getModeSemitones() - 12.0f) < 0.001f, "WAIL HOLD did not stay at the top");
            }
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
        {   // Pressing the WAIL lock in the middle of a note glides there; it never jumps.
            uint64_t holdAge = 0;
            SealVoice wail(13, holdAge);
            MacroState m;
            m.behaviorMode = BehaviourMode::wail;
            m.wailBend = 12.0f;
            m.wailHold = false;
            wail.prepare(48000, 480);
            wail.setMacros(m);
            wail.startNote(0, 60, 1, 8192);
            AudioBuffer block(2, 480);
            float last = 0.0f, worst = 0.0f;
            for (int i = 0; i < 400; ++i)
            {
                if (i == 300) { m.wailHold = true; wail.setMacros(m); }     // at beat 6, the swing's bottom
                wail.renderNextBlock(block, 0, 480);
                const auto st = wail.getModeSemitones();
                if (i > 0) worst = std::max(worst, std::abs(st - last));
                last = st;
            }
            check(worst < 3.0f && std::abs(last - 12.0f) < 0.001f, "WAIL lock jumped instead of gliding");
        }
        {   // ARP: C-E-G held, UP at 1/8 and 120 BPM (11025 samples a step at 44.1 kHz): C, then E exactly a step later,
            // one note at a time; letting go stops new steps; HOLD keeps them coming after the keys are up.
            for (const bool hold : { false, true })
            {
                auto owned = std::make_unique<PhoqerEngine>();
                auto& engine = *owned;
                AudioBuffer buffer(2, 441);
                CharacterPreset loaded;
                MacroState m;
                m.space = 0;
                m.loadedVoice = &loaded;
                m.tempoBpm = 120;
                m.arp.on = true;
                m.arp.rate = 1;
                m.arp.hold = hold;
                engine.prepare(44100, 441, 2);
                std::vector<MidiEvent> chord { { MidiEventType::noteOn, 0, 0, 48, 0.8f }, { MidiEventType::noteOn, 0, 0, 52, 0.8f }, { MidiEventType::noteOn, 0, 0, 55, 0.8f } };
                engine.process(buffer, chord.data(), static_cast<int>(chord.size()), m, 0);
                check(engine.getNewestPitchNote() == 48 && engine.getActiveVoiceCount() == 1, "ARP did not start with the lowest key alone");
                for (int b = 1; b < 25; ++b) engine.process(buffer, nullptr, 0, m, 0);
                check(engine.getNewestPitchNote() == 48, "ARP stepped early");
                engine.process(buffer, nullptr, 0, m, 0);          // block 25 starts on sample 11025: the second step
                check(engine.getNewestPitchNote() == 52, "ARP did not step to E on time");
                check(engine.getActiveVoiceCount() <= 2, "ARP played the chord together");
                std::vector<MidiEvent> up { { MidiEventType::noteOff, 0, 0, 48, 0 }, { MidiEventType::noteOff, 0, 0, 52, 0 }, { MidiEventType::noteOff, 0, 0, 55, 0 } };
                engine.process(buffer, up.data(), static_cast<int>(up.size()), m, 0);
                for (int b = 0; b < 30; ++b) engine.process(buffer, nullptr, 0, m, 0);    // past the third step's time
                check(hold ? engine.getNewestPitchNote() == 55 : engine.getNewestPitchNote() == 52, hold ? "ARP HOLD stopped" : "ARP kept stepping after the keys were up");
            }
        }
        {   // NITRO's cap: 8 keys with three extra notes each play 16 voices at most, and they all end with their keys.
            auto owned = std::make_unique<PhoqerEngine>();
            auto& engine = *owned;
            AudioBuffer buffer(2, 128);
            CharacterPreset loaded;
            MacroState m;
            m.space = 0;
            m.loadedVoice = &loaded;
            m.nitro.on = true;
            m.nitro.offsets = { 7, 12, 19 };
            engine.prepare(48000, 128, 2);
            std::vector<MidiEvent> keys;
            for (int k = 0; k < 8; ++k) keys.push_back({ MidiEventType::noteOn, 0, 0, 48 + k * 3, 0.8f });
            engine.process(buffer, keys.data(), static_cast<int>(keys.size()), m, 0);
            settle(engine, buffer, m, 40);     // past the strum (3 x 12 ms) and the 3 ms fades
            check(engine.getActiveVoiceCount() == 16, "NITRO did not stop at 16 voices");
            for (int k = 0; k < 8; ++k) event(engine, buffer, m, MidiEventType::noteOff, 48 + k * 3, 0);
            settle(engine, buffer, m);
            check(engine.getActiveVoiceCount() == 0, "NITRO notes hung after the keys came up");
        }
        std::cout << "PERFORMANCE_SANITY_OK: poly glide, chords, bend, wail bend, wail hold, wail lock glide, ARP steps and HOLD, NITRO cap, smoothing, limiter\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
