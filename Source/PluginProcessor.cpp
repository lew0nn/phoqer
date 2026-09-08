#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
juce::NormalisableRange<float> makeRange(float minimum, float maximum, float interval)
{
    return { minimum, maximum, interval };
}
}

PhoqerAudioProcessor::PhoqerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PHOQER_STATE", createParameterLayout())
{
    boom = requireParameter(parameters, "boom");
    air = requireParameter(parameters, "air");
    bark = requireParameter(parameters, "bark");
    vowel = requireParameter(parameters, "vowel");
    space = requireParameter(parameters, "space");
    tide = requireParameter(parameters, "tide");
    detune = requireParameter(parameters, "detune");
    output = requireParameter(parameters, "output");
    character = requireParameter(parameters, "character");
    behavior = requireParameter(parameters, "behavior");
}

std::atomic<float>* PhoqerAudioProcessor::requireParameter(
    juce::AudioProcessorValueTreeState& state, const char* id)
{
    auto* value = state.getRawParameterValue(id);
    jassert(value != nullptr);
    return value;
}

juce::AudioProcessorValueTreeState::ParameterLayout PhoqerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using ParameterId = juce::ParameterID;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "boom", 1 }, "BOOM", makeRange(0.0f, 1.0f, 0.001f), 0.50f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "air", 1 }, "AIR", makeRange(0.0f, 1.0f, 0.001f), 0.25f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "bark", 1 }, "BARK", makeRange(0.0f, 1.0f, 0.001f), 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "vowel", 1 }, "VOWEL", makeRange(0.0f, 1.0f, 0.001f), 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "space", 1 }, "REVERB", makeRange(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "tide", 1 }, "TIDE", makeRange(0.0f, 1.0f, 0.001f), 0.25f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "detune", 1 }, "DETUNE", makeRange(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "output", 1 }, "OUTPUT", makeRange(-24.0f, 18.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterId { "character", 1 }, "CHARACTER",
        juce::StringArray { "BURP", "DEFAULT", "SYNTH" }, 1));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterId { "behavior", 1 }, "BEHAVIOR",
        juce::StringArray { "CALL", "HONK", "BARK", "WAIL", "MURMUR" }, 0));
    return layout;
}

void PhoqerAudioProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    engine.prepare(sampleRate, maximumExpectedSamplesPerBlock,
                   std::max(1, getTotalNumOutputChannels()));
}

void PhoqerAudioProcessor::releaseResources()
{
    engine.reset();
    midiEventCount = 0;
}

bool PhoqerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PhoqerAudioProcessor::translateMidi(const juce::MidiBuffer& midiMessages) noexcept
{
    midiEventCount = 0;
    for (const auto metadata : midiMessages)
    {
        if (midiEventCount >= static_cast<int>(midiEvents.size()))
            break;

        const auto& message = metadata.getMessage();
        const int channel = std::clamp(message.getChannel() - 1, 0, 15);
        const int offset = std::max(0, metadata.samplePosition);
        phoqer::MidiEvent event;
        event.sampleOffset = offset;
        event.channel = channel;

        if (message.isNoteOn())
        {
            event.type = phoqer::MidiEventType::noteOn;
            event.note = message.getNoteNumber();
            event.value = message.getFloatVelocity();
        }
        else if (message.isNoteOff())
        {
            event.type = phoqer::MidiEventType::noteOff;
            event.note = message.getNoteNumber();
            event.value = message.getFloatVelocity();
        }
        else if (message.isPitchWheel())
        {
            event.type = phoqer::MidiEventType::pitchWheel;
            event.value = std::clamp((static_cast<float>(message.getPitchWheelValue()) - 8192.0f)
                                         / 8192.0f,
                                     -1.0f, 1.0f);
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            event.type = phoqer::MidiEventType::allNotesOff;
        }
        else
        {
            continue;
        }

        midiEvents[static_cast<size_t>(midiEventCount++)] = event;
    }
}

void PhoqerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    translateMidi(midiMessages);

    phoqer::MacroState macros;
    macros.boom = boom->load(std::memory_order_relaxed);
    macros.air = air->load(std::memory_order_relaxed);
    macros.bark = bark->load(std::memory_order_relaxed);
    macros.vowel = vowel->load(std::memory_order_relaxed);
    macros.space = space->load(std::memory_order_relaxed);
    macros.tide = tide->load(std::memory_order_relaxed);
    macros.detune = detune->load(std::memory_order_relaxed);
    macros.character = static_cast<phoqer::SealCharacter>(
        std::clamp(static_cast<int>(std::lround(character->load(std::memory_order_relaxed))), 0, 2));
    macros.behaviorMode = static_cast<phoqer::BehaviourMode>(
        std::clamp(static_cast<int>(std::lround(behavior->load(std::memory_order_relaxed))), 0, 4));

    phoqer::AudioBuffer outputBuffer(const_cast<float**>(buffer.getArrayOfWritePointers()),
                                     buffer.getNumChannels(),
                                     buffer.getNumSamples());
    engine.process(outputBuffer, midiEvents.data(), midiEventCount, macros,
                   output->load(std::memory_order_relaxed));
    midiEventCount = 0;
}

void PhoqerAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destinationData);
}

void PhoqerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* PhoqerAudioProcessor::createEditor()
{
    return new PhoqerAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhoqerAudioProcessor();
}
