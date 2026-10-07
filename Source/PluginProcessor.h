#pragma once

#include <JuceHeader.h>

#include "Core/MidiEvent.h"
#include "Core/PhoqerEngine.h"

#include "UI/Recorder.h"

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
    double getTailLengthSeconds() const override { return 5.0; }   // REVERB at full rings for about 4 s

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

    // Notes played from the editor's on-screen piano and computer keyboard; merged into each block.
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    // EDIT > PANIC: every voice on every channel is released at the start of the next block.
    void panic() noexcept { panicRequested.store(true, std::memory_order_relaxed); }

    // Records the output to WAV for the editor's REC button.
    phoqer::ui::Recorder& getRecorder() noexcept { return recorder; }

private:
    void translateMidi(const juce::MidiBuffer& midiMessages) noexcept;
    static std::atomic<float>* requireParameter(juce::AudioProcessorValueTreeState& state,
                                                const char* id);

    phoqer::PhoqerEngine engine;
    std::array<phoqer::MidiEvent, 256> midiEvents {};
    int midiEventCount = 0;
    juce::MidiKeyboardState keyboardState;
    std::atomic<bool> panicRequested { false };
    phoqer::ui::Recorder recorder;

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
