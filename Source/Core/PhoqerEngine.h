#pragma once

#include "AudioBuffer.h"
#include "FaceTelemetry.h"
#include "MidiEvent.h"
#include "PhoqerTypes.h"
#include "../DSP/CheapSpace.h"
#include "../DSP/Chorus.h"
#include "../DSP/OutputStage.h"
#include "../Voice/SealVoice.h"

#include <array>
#include <cstdint>

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

private:
    SealVoice* findVoiceForNoteOn() noexcept;
    void handleEvent(const MidiEvent& event) noexcept;
    void releaseNote(int channel, int note) noexcept;
    void renderVoices(AudioBuffer& output, int startSample, int numSamples);
    void publishTelemetry(const AudioBuffer& output) noexcept;

    uint64_t ageCounter = 0;
    std::array<SealVoice, voiceCount> voices;
    std::array<int, 16> pitchWheels {};
    std::array<bool, 16> sustainDown {};                 // the sustain pedal, per MIDI channel
    std::array<bool, voiceCount> sustained {};           // a voice whose key is up but the pedal holds it
    float modWheel = 0.0f;                               // 0..1, adds to TIDE
    Chorus chorusStage;      // DETUNE
    CheapSpace spaceStage;   // REVERB
    OutputStage outputStage;
    TelemetryPublisher telemetry;
    SealCharacter activeCharacter = defaultSealCharacter;
};
}
