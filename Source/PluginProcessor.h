#pragma once

#include "Core/PhoqerEngine.h"
#include "Core/PhoqerParameters.h"
#include <JuceHeader.h>

class PhoqerAudioProcessor final : public juce::AudioProcessor
{
public:
    PhoqerAudioProcessor();
    ~PhoqerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    const phoqer::TelemetryPublisher& getTelemetry() const noexcept { return engine.getTelemetry(); }
    phoqer::PhoqerEngine& getEngineForDiagnostics() noexcept { return engine; }

private:
    juce::AudioProcessorValueTreeState parameters;
    phoqer::PhoqerEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhoqerAudioProcessor)
};
