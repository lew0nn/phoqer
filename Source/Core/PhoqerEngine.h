#pragma once

#include "FaceTelemetry.h"
#include "PhoqerParameters.h"
#include "PhoqerTypes.h"
#include "../DSP/CheapDigitalStage.h"
#include "../DSP/CheapSpace.h"
#include "../DSP/OutputStage.h"
#include "../Voice/SealSound.h"
#include "../Voice/SealVoice.h"
#include <JuceHeader.h>

namespace phoqer
{
class PhoqerSynthesiser final : public juce::Synthesiser
{
protected:
    juce::SynthesiserVoice* findVoiceToSteal(juce::SynthesiserSound* sound,
                                              int midiChannel, int midiNoteNumber) const override;
};

class PhoqerEngine
{
public:
    explicit PhoqerEngine(juce::AudioProcessorValueTreeState& state);

    void prepare(double sampleRate, int maximumBlockSize, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& output, juce::MidiBuffer& midi);

    const TelemetryPublisher& getTelemetry() const noexcept { return telemetry; }

private:
    MacroState readMacros() const noexcept;
    void publishTelemetry(const juce::AudioBuffer<float>& output) noexcept;

    juce::AudioProcessorValueTreeState& parameters;
    std::atomic<float>* boomParameter = nullptr;
    std::atomic<float>* airParameter = nullptr;
    std::atomic<float>* barkParameter = nullptr;
    std::atomic<float>* vowelParameter = nullptr;
    std::atomic<float>* spaceParameter = nullptr;
    std::atomic<float>* tideParameter = nullptr;
    std::atomic<float>* outputParameter = nullptr;

    PhoqerSynthesiser synth;
    CheapDigitalStage digitalStage;
    CheapSpace spaceStage;
    OutputStage outputStage;
    TelemetryPublisher telemetry;
    uint64_t ageCounter = 0;
    int waveformDecimation = 44;
    int waveformCountdown = 0;
};
}
