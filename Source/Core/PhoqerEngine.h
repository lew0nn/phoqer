#pragma once

#include "AudioBuffer.h"
#include "FaceTelemetry.h"
#include "MidiEvent.h"
#include "PhoqerTypes.h"
#include "../DSP/CheapSpace.h"
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

private:
    SealVoice* findVoiceForNoteOn() noexcept;
    void handleEvent(const MidiEvent& event) noexcept;
    void renderVoices(AudioBuffer& output, int startSample, int numSamples);
    void publishTelemetry(const AudioBuffer& output) noexcept;

    uint64_t ageCounter = 0;
    std::array<SealVoice, voiceCount> voices;
    std::array<int, 16> pitchWheels {};
    CheapSpace spaceStage;
    OutputStage outputStage;
    TelemetryPublisher telemetry;
    SealCharacter activeCharacter = defaultSealCharacter;
    int waveformDecimation = 44;
    int waveformCountdown = 0;
};
}
