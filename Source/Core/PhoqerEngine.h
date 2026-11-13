#pragma once

#include "AudioBuffer.h"
#include "FaceTelemetry.h"
#include "MidiEvent.h"
#include "PhoqerTypes.h"
#include "../DSP/FdnSpace.h"
#include "../DSP/Chorus.h"
#include "../DSP/FishSong.h"
#include "../DSP/OutputStage.h"
#include "../Voice/SealVoice.h"

#include <array>
#include <cstdint>
#include <utility>

namespace phoqer
{
class PhoqerEngine
{
public:
    PhoqerEngine();

    void prepare(double sampleRate, int maximumBlockSize, int numChannels);
    void reset();
    void process(AudioBuffer& output, const MidiEvent* events, int eventCount,
                 const MacroState& macros, float outputDecibels);

    const TelemetryPublisher& getTelemetry() const noexcept { return telemetry; }
    SealCharacter getActiveCharacter() const noexcept { return activeCharacter; }
    int getLatencySamples() const noexcept { return outputStage.getLatencySamples(); }
    int getActiveVoiceCount() const noexcept;
    float getNewestPitchNote() const noexcept;     // the pitch of the most recently started note, mid-glide included

private:
    SealVoice* findVoiceForNoteOn(int usableSlots, int playingLimit) noexcept;
    SealVoice* leastImportantVoice(int usableSlots) noexcept;
    void handleEvent(const MidiEvent& event, int sampleInBlock) noexcept;
    void releaseNote(int channel, int note) noexcept;
    void renderVoices(AudioBuffer& output, int startSample, int numSamples);
    // Renders up to target, starting NITRO's strummed notes on their samples on the way.
    void renderTo(AudioBuffer& output, int& cursor, int target);
    void renderSegment(AudioBuffer& output, int startSample, int numSamples);     // YAP moves VOWEL every 64 samples
    void queueNitro(int channel, int note, float velocity, int sampleInBlock) noexcept;
    bool nitroActive() const noexcept;
    bool yapActive() const noexcept { return macros.yap && macros.loadedVoice != nullptr; }
    // ARP (KYRAX only): the held keys are collected instead of played, and a clock at the tempo plays them one
    // at a time through the normal note path (so NITRO, YAP and GLIDE apply to every step).
    bool arpActive() const noexcept { return macros.arp.on && macros.loadedVoice != nullptr; }
    void arpKey(const MidiEvent&, int sampleInBlock) noexcept;     // a key goes down or up while ARP is on
    void arpRun(AudioBuffer& output, int& cursor, int target);     // renders to target, playing the steps due on the way
    void arpClear(int sampleInBlock) noexcept;                     // ARP switched off or panic: the step note ends, keys forgotten
    int arpSequence(std::array<int, 48>& out) const noexcept;      // the notes one cycle plays, from the keys, pattern and octaves
    double arpStepSamples() const noexcept;
    struct ArpKey { int note = 0, channel = 0, order = 0; float velocity = 0; bool down = false; };
    std::array<ArpKey, 16> arpKeys {};
    int arpKeyCount = 0, arpOrder = 0, arpStep = 0;
    double arpGrid = 0.0;                         // where the last step sat on the beat grid (samples since prepare), swing aside
    int64_t arpNextAt = -1, arpOffAt = -1;        // the next step and the sounding note's end, samples since prepare; -1 none
    int arpSounding = -1, arpSoundingChannel = 0;
    uint32_t arpRandom = 0x2545f491u;
    bool arpWasActive = false;
    template <size_t... I>
    static std::array<SealVoice, sizeof...(I)> makeVoices(uint64_t& age, std::index_sequence<I...>)
    {
        return {{ SealVoice(0x91e10da5u + static_cast<uint32_t>(I) * 0x9e3779b9u, age)... }};
    }
    void publishTelemetry(const AudioBuffer& output) noexcept;

    uint64_t ageCounter = 0;
    // voiceCount notes sound at once; the one spare slot lets a taken-over note fade out (3 ms) while the new one starts.
    // NITRO plays up to four voices per key, so it gets eight keys' worth of slots; without it only the first
    // plainSlots are used, exactly as before.
    static constexpr int plainSlots = voiceCount + 1;
    static constexpr int voiceSlots = voiceCount * (1 + nitroNotes) + 1;
    // NITRO plays 16 voices at most (about four keys with a full stack). Past that the oldest extra note
    // fades out for the new one; a key is only given up when no extra note is left.
    static constexpr int nitroVoiceLimit = 16;
    SealVoice* nitroVoiceFor() noexcept;
    std::array<SealVoice, voiceSlots> voices;
    struct PendingNote { int64_t at = 0; int channel = 0, root = 0, note = 0; float velocity = 0, gain = 1, pan = 0, cents = 0; bool used = false, released = false, sustained = false; };
    std::array<PendingNote, 64> pending {};     // NITRO's strummed notes waiting for their sample
    int64_t clock = 0, blockStart = 0;          // samples since prepare
    double sampleRate = 44100.0;
    MacroState macros;                          // this block's knobs (YAP moves VOWEL in it)
    float yapVowel = 0.35f;
    double yapBeats = 0.0;
    std::array<int, 16> pitchWheels {};
    std::array<bool, 16> sustainDown {};                 // the sustain pedal, per MIDI channel
    std::array<bool, voiceSlots> sustained {};           // a voice whose key is up but the pedal holds it
    float modWheel = 0.0f;                               // 0..1, adds to TIDE
    const SealVoice* newestVoice = nullptr;           // the last note started: where a glide starts from
    Chorus chorusStage;      // DETUNE
    FdnSpace spaceStage;     // REVERB (SPACE)
    OutputStage outputStage;
    FishSong fishSong;       // the fed seal sings the fish
    int fishSongs = 0;
    TelemetryPublisher telemetry;
    SealCharacter activeCharacter = defaultSealCharacter;
};
}
