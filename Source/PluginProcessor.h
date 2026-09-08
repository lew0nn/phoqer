#pragma once

#include <JuceHeader.h>

#include "Core/MidiEvent.h"
#include "Core/PhoqerEngine.h"

#include <array>
#include <atomic>

class PhoqerAudioProcessor final : public juce::AudioProcessor
{
public:
    PhoqerAudioProcessor();
    ~PhoqerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }
    const phoqer::TelemetryPublisher& getTelemetry() const noexcept { return engine.getTelemetry(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void translateMidi(const juce::MidiBuffer& midiMessages) noexcept;
    static std::atomic<float>* requireParameter(juce::AudioProcessorValueTreeState& state,
                                                const char* id);

    phoqer::PhoqerEngine engine;
    std::array<phoqer::MidiEvent, 256> midiEvents {};
    int midiEventCount = 0;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* boom = nullptr;
    std::atomic<float>* air = nullptr;
    std::atomic<float>* bark = nullptr;
    std::atomic<float>* vowel = nullptr;
    std::atomic<float>* space = nullptr;
    std::atomic<float>* tide = nullptr;
    std::atomic<float>* detune = nullptr;
    std::atomic<float>* output = nullptr;
    std::atomic<float>* character = nullptr;
    std::atomic<float>* behavior = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhoqerAudioProcessor)
};
