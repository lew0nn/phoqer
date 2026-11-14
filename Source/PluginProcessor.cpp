#include "PluginProcessor.h"
#include "UI/Editor.h"

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
    for (auto& lane : arpLanes) lane.store(1000u | (1000u << 10), std::memory_order_relaxed);     // every step full
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
    standaloneTempo = requireParameter(parameters, "standaloneTempo");
    glide = requireParameter(parameters, "glide");
    wailBend = requireParameter(parameters, "wailBend");
    wailHold = requireParameter(parameters, "wailHold");
    metronomeEnabled = requireParameter(parameters, "metronomeEnabled");
    metronomeVolume = requireParameter(parameters, "metronomeVolume");
    metronomeBeatsPerBar = requireParameter(parameters, "metronomeBeats");
    metronomeAccent = requireParameter(parameters, "metronomeAccent");
    metronomeSound = requireParameter(parameters, "metronomeSound");
    countIn = requireParameter(parameters, "countIn");
    yap = requireParameter(parameters, "yap");
    nitro = requireParameter(parameters, "nitro");
    for (size_t k = 0; k < nitroNote.size(); ++k)
    {
        nitroNote[k] = requireParameter(parameters, ("nitroNote" + juce::String(k + 1)).toRawUTF8());
        nitroLevel[k] = requireParameter(parameters, ("nitroLevel" + juce::String(k + 1)).toRawUTF8());
    }
    nitroWidth = requireParameter(parameters, "nitroWidth");
    nitroStrum = requireParameter(parameters, "nitroStrum");
    nitroDetune = requireParameter(parameters, "nitroDetune");
    arp = requireParameter(parameters, "arp");
    arpRate = requireParameter(parameters, "arpRate");
    arpPattern = requireParameter(parameters, "arpPattern");
    arpHold = requireParameter(parameters, "arpHold");
    arpGate = requireParameter(parameters, "arpGate");
    arpSwing = requireParameter(parameters, "arpSwing");
    arpOctaves = requireParameter(parameters, "arpOctaves");
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
    constexpr int defaultCharacter = 0;    // PHOQER opens on BURP
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterId { "character", 1 }, "CHARACTER",
        juce::StringArray { "BURP", "SQUEAL", "GROAN" }, defaultCharacter));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterId { "behavior", 1 }, "BEHAVIOR",
        juce::StringArray { "CALL", "HONK", "BARK", "WAIL", "MURMUR" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "standaloneTempo", 1 }, "STANDALONE TEMPO", makeRange(20.0f, 400.0f, 1.0f), 120.0f,
        juce::AudioParameterFloatAttributes().withLabel("BPM").withAutomatable(false)));
    {
        // GLIDE: a note played over a held one slides in from its pitch. Half a turn is 250 ms.
        juce::NormalisableRange<float> range(0.0f, 2000.0f, 1.0f);
        range.setSkewForCentre(250.0f);
        layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterId { "glide", 1 }, "GLIDE", range, 0.0f,
                                                               juce::AudioParameterFloatAttributes().withLabel("ms")));
    }
    // WAIL BEND: how far WAIL sweeps up and down, set in a small panel on the WAIL button. 2.4 semitones is how WAIL always sounded.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "wailBend", 1 }, "WAIL BEND", makeRange(0.0f, 24.0f, 0.1f), 2.4f,
        juce::AudioParameterFloatAttributes().withLabel("st")));
    // The padlock beside it: WAIL rises to the bend and stays there.
    layout.add(std::make_unique<juce::AudioParameterBool>(ParameterId { "wailHold", 1 }, "WAIL HOLD", false));
    // The standalone app's metronome (METRO.EXE). A DAW has its own click, so none of these are
    // automatable, and presets leave them alone.
    layout.add(std::make_unique<juce::AudioParameterBool>(
        ParameterId { "metronomeEnabled", 1 }, "STANDALONE METRONOME", false,
        juce::AudioParameterBoolAttributes().withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "metronomeVolume", 1 }, "METRONOME VOLUME", makeRange(0.0f, 100.0f, 1.0f), 70.0f,
        juce::AudioParameterFloatAttributes().withLabel("%").withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterId { "metronomeBeats", 1 }, "BEATS PER BAR", juce::StringArray { "2/4", "3/4", "4/4", "5/4", "6/4", "7/4" }, 2,
        juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterBool>(ParameterId { "metronomeAccent", 1 }, "ACCENT BEAT 1", true,
        juce::AudioParameterBoolAttributes().withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterId { "metronomeSound", 1 }, "METRONOME SOUND", juce::StringArray { "BEEP", "WOOD", "SEAL" }, 0,
        juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterId { "countIn", 1 }, "COUNT-IN", juce::StringArray { "OFF", "1 BAR", "2 BARS" }, 0,
        juce::AudioParameterChoiceAttributes().withAutomatable(false)));

    // KYRAX only (the free voices ignore them). YAP: the seal talks in 8ths at the tempo. NITRO: every key
    // plays up to three more notes; right-click NITRO for NITRO.EXE. Named KYRAX so a DAW's list says whose
    // they are; only the two switches can be automated, the NITRO.EXE details are set in the window.
    layout.add(std::make_unique<juce::AudioParameterBool>(ParameterId { "yap", 1 }, "KYRAX YAP", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(ParameterId { "nitro", 1 }, "KYRAX NITRO", false));
    constexpr int defaultOffsets[] { 7, 12, 0 };
    const auto fixed = [] { return juce::AudioParameterFloatAttributes().withAutomatable(false); };
    for (int k = 0; k < phoqer::nitroNotes; ++k)
    {
        const auto n = juce::String(k + 1);
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParameterId { "nitroNote" + n, 1 }, "KYRAX NITRO NOTE " + n, -24, 24, defaultOffsets[k],
            juce::AudioParameterIntAttributes().withAutomatable(false).withLabel("st").withStringFromValueFunction(
                [](int v, int) { return v == 0 ? juce::String("OFF") : (v > 0 ? "+" : "") + juce::String(v); })));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterId { "nitroLevel" + n, 1 }, "KYRAX NITRO LEVEL " + n, makeRange(0.0f, 1.0f, 0.01f), 0.7f, fixed()));
    }
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "nitroWidth", 1 }, "KYRAX NITRO WIDTH", makeRange(0.0f, 1.0f, 0.01f), 0.6f, fixed()));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "nitroStrum", 1 }, "KYRAX NITRO STRUM", makeRange(0.0f, 100.0f, 1.0f), 12.0f,
        fixed().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterId { "nitroDetune", 1 }, "KYRAX NITRO DETUNE", makeRange(0.0f, 50.0f, 0.5f), 0.0f,
        fixed().withLabel("ct")));
    // ARP: the switch can be automated; its settings are set in ARP.EXE (right-click ARP).
    layout.add(std::make_unique<juce::AudioParameterBool>(ParameterId { "arp", 1 }, "KYRAX ARP", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(ParameterId { "arpRate", 1 }, "KYRAX ARP RATE",
        juce::StringArray { "1/4", "1/8", "1/16", "1/32", "1/8T" }, 2, juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterChoice>(ParameterId { "arpPattern", 1 }, "KYRAX ARP PATTERN",
        juce::StringArray { "UP", "DOWN", "UP + DOWN", "RANDOM", "AS PLAYED" }, 0, juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterBool>(ParameterId { "arpHold", 1 }, "KYRAX ARP HOLD", false,
        juce::AudioParameterBoolAttributes().withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterId { "arpGate", 1 }, "KYRAX ARP GATE", makeRange(0.05f, 1.0f, 0.01f), 0.6f, fixed()));
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterId { "arpSwing", 1 }, "KYRAX ARP SWING", makeRange(0.0f, 1.0f, 0.01f), 0.0f, fixed()));
    layout.add(std::make_unique<juce::AudioParameterInt>(ParameterId { "arpOctaves", 1 }, "KYRAX ARP OCTAVES", 1, 3, 1,
        juce::AudioParameterIntAttributes().withAutomatable(false).withLabel("oct")));
    return layout;
}

void PhoqerAudioProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    engine.prepare(sampleRate, maximumExpectedSamplesPerBlock,
                   std::max(1, getTotalNumOutputChannels()));
    setLatencySamples(engine.getLatencySamples());     // the safety limiter looks 2 ms ahead
    metronome.prepare(sampleRate);
}

void PhoqerAudioProcessor::releaseResources()
{
    engine.reset();
    metronome.reset();
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
        {
            // A dense MIDI burst must not discard note-offs and leave voices stuck.
            midiEventCount = 0;
            for (int channel = 0; channel < 16; ++channel)
                midiEvents[static_cast<size_t>(midiEventCount++)] = { phoqer::MidiEventType::allNotesOff, 0, channel, 0, 0 };
            break;
        }

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
            lastNote.store(event.note, std::memory_order_relaxed);
            lastVelocity.store(message.getVelocity(), std::memory_order_relaxed);
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
        else if (message.isSustainPedalOn() || message.isSustainPedalOff())
        {
            event.type = phoqer::MidiEventType::sustainPedal;
            event.value = message.isSustainPedalOn() ? 1.0f : 0.0f;
        }
        else if (message.isControllerOfType(1))
        {
            event.type = phoqer::MidiEventType::modWheel;
            event.value = static_cast<float>(message.getControllerValue()) / 127.0f;
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
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    translateMidi(midiMessages);
    if (panicRequested.exchange(false, std::memory_order_relaxed))
    {
        midiEventCount = 0;
        for (int channel = 0; channel < 16 && midiEventCount < static_cast<int>(midiEvents.size()); ++channel)
        {
            phoqer::MidiEvent off;
            off.type = phoqer::MidiEventType::allNotesOff;
            off.channel = channel;
            off.sampleOffset = 0;
            midiEvents[static_cast<size_t>(midiEventCount++)] = off;
        }
    }

    // Fed the fish: a gulp and the fish song, or overfed: a gulp and a belch, in the seal's own register.
    for (auto* request : { &fedRequested, &belchRequested })
        if (request->exchange(false, std::memory_order_relaxed) && midiEventCount < static_cast<int>(midiEvents.size()))
        {
            constexpr int treatNote[] { 48, 72, 69 };     // BURP, SQUEAL, GROAN
            phoqer::MidiEvent fed;
            fed.type = request == &fedRequested ? phoqer::MidiEventType::treat : phoqer::MidiEventType::belch;
            fed.note = treatNote[juce::jlimit(0, 2, static_cast<int>(std::lround(character->load(std::memory_order_relaxed))))];
            fed.value = 0.9f;
            midiEvents[static_cast<size_t>(midiEventCount++)] = fed;
        }

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
    // No waiting, allocation or file access in the audio callback.
    macros.loadedVoice = voices.audioVoice();
    macros.glideMs = glide->load(std::memory_order_relaxed);
    macros.wailBend = wailBend->load(std::memory_order_relaxed);
    macros.wailHold = wailHold->load(std::memory_order_relaxed) >= 0.5f;
    macros.yap = yap->load(std::memory_order_relaxed) >= 0.5f;
    macros.nitro.on = nitro->load(std::memory_order_relaxed) >= 0.5f;
    for (size_t k = 0; k < nitroNote.size(); ++k)
    {
        macros.nitro.offsets[k] = static_cast<int>(std::lround(nitroNote[k]->load(std::memory_order_relaxed)));
        macros.nitro.levels[k] = nitroLevel[k]->load(std::memory_order_relaxed);
    }
    macros.nitro.width = nitroWidth->load(std::memory_order_relaxed);
    macros.nitro.strumMs = nitroStrum->load(std::memory_order_relaxed);
    macros.nitro.detuneCents = nitroDetune->load(std::memory_order_relaxed);
    macros.arp.on = arp->load(std::memory_order_relaxed) >= 0.5f;
    macros.arp.rate = juce::jlimit(0, 4, static_cast<int>(std::lround(arpRate->load(std::memory_order_relaxed))));
    macros.arp.pattern = juce::jlimit(0, 4, static_cast<int>(std::lround(arpPattern->load(std::memory_order_relaxed))));
    macros.arp.hold = arpHold->load(std::memory_order_relaxed) >= 0.5f;
    macros.arp.gate = arpGate->load(std::memory_order_relaxed);
    macros.arp.swing = arpSwing->load(std::memory_order_relaxed);
    macros.arp.octaves = juce::jlimit(1, 3, static_cast<int>(std::lround(arpOctaves->load(std::memory_order_relaxed))));
    if (wrapperType == wrapperType_Standalone || wrapperType == wrapperType_Undefined)
        macros.tempoBpm = standaloneTempo->load(std::memory_order_relaxed);
    macros.songPlaying = false;
    if (auto* head = getPlayHead())                       // hosted tempo takes priority over the standalone BPM field
        if (const auto position = head->getPosition())
        {
            if (const auto bpm = position->getBpm())
                macros.tempoBpm = static_cast<float>(*bpm);
            if (const auto beats = position->getPpqPosition(); beats && position->getIsPlaying())
            {
                macros.songPlaying = true;                // ARP and YAP follow the song's beat grid
                macros.songBeats = *beats;
            }
        }

    phoqer::AudioBuffer outputBuffer(const_cast<float**>(buffer.getArrayOfWritePointers()),
                                     buffer.getNumChannels(),
                                     buffer.getNumSamples());
    for (int i = 0; i < phoqer::arpLaneSteps; ++i)
        macros.arp.lanes[static_cast<size_t>(i)] = getArpLane(i);
    engine.process(outputBuffer, midiEvents.data(), midiEventCount, macros,
                   output->load(std::memory_order_relaxed));
    playingArpLane.store(engine.getArpLane(), std::memory_order_relaxed);
    midiEventCount = 0;
    singing.store(engine.getActiveVoiceCount() > 0, std::memory_order_relaxed);
    // A count-in starts the click from beat 1 in the same block the recorder starts skipping it.
    const bool countingIn = recorder.countInRemaining() > 0;
    if (countingIn && ! wasCountingIn) metronome.restart();
    wasCountingIn = countingIn;
    recorder.process(buffer);
    const bool standalone = wrapperType == wrapperType_Standalone || wrapperType == wrapperType_Undefined;
    phoqer::MetronomeSettings settings;
    settings.volume = metronomeVolume->load(std::memory_order_relaxed) * 0.01f;
    settings.beatsPerBar = getMetronomeBeatsPerBar();
    settings.accent = metronomeAccent->load(std::memory_order_relaxed) >= 0.5f;
    settings.sound = static_cast<int>(metronomeSound->load(std::memory_order_relaxed));
    metronome.process(outputBuffer, standalone && (countingIn || metronomeEnabled->load(std::memory_order_relaxed) >= 0.5f),
                      standaloneTempo->load(std::memory_order_relaxed), settings);
    metronomeBeats.store(static_cast<float>(metronome.getBeats()), std::memory_order_relaxed);
}

namespace
{
uint32_t packLane(phoqer::ArpLaneStep s) noexcept
{
    const auto thousandths = [](float v) { return static_cast<uint32_t>(std::lround(juce::jlimit(0.0f, 1.0f, v) * 1000.0f)); };
    return thousandths(s.velocity) | (thousandths(s.gate) << 10) | (s.rest ? (1u << 20) : 0u);
}
phoqer::ArpLaneStep unpackLane(uint32_t bits) noexcept
{
    return { static_cast<float>(bits & 1023u) / 1000.0f, static_cast<float>((bits >> 10) & 1023u) / 1000.0f, (bits & (1u << 20)) != 0 };
}
std::array<phoqer::ArpLaneStep, phoqer::arpLaneSteps> parseLanes(const juce::String& text)
{
    std::array<phoqer::ArpLaneStep, phoqer::arpLaneSteps> lanes {};     // anything missing or broken: full
    const auto steps = juce::StringArray::fromTokens(text, ";", "");
    for (int i = 0; i < juce::jmin(steps.size(), phoqer::arpLaneSteps); ++i)
    {
        const auto parts = juce::StringArray::fromTokens(steps[i], ",", "");
        if (parts.size() != 3) continue;
        auto& s = lanes[static_cast<size_t>(i)];
        s.velocity = juce::jlimit(0.05f, 1.0f, parts[0].getFloatValue());
        s.gate = juce::jlimit(0.05f, 1.0f, parts[1].getFloatValue());
        s.rest = parts[2].getIntValue() != 0;
    }
    return lanes;
}
juce::String formatLanes(const std::array<phoqer::ArpLaneStep, phoqer::arpLaneSteps>& lanes)
{
    juce::StringArray steps;
    for (const auto& s : lanes)
        steps.add(juce::String(s.velocity, 2) + "," + juce::String(s.gate, 2) + "," + (s.rest ? "1" : "0"));
    return steps.joinIntoString(";");
}
}

phoqer::ArpLaneStep PhoqerAudioProcessor::getArpLane(int step) const noexcept
{
    return unpackLane(arpLanes[static_cast<size_t>(juce::jlimit(0, phoqer::arpLaneSteps - 1, step))].load(std::memory_order_relaxed));
}

void PhoqerAudioProcessor::setArpLane(int step, phoqer::ArpLaneStep s) noexcept
{
    s.velocity = juce::jlimit(0.05f, 1.0f, s.velocity);
    s.gate = juce::jlimit(0.05f, 1.0f, s.gate);
    arpLanes[static_cast<size_t>(juce::jlimit(0, phoqer::arpLaneSteps - 1, step))].store(packLane(s), std::memory_order_relaxed);
    parameters.state.setProperty("arpLanes", saveArpLanes(), nullptr);
}

void PhoqerAudioProcessor::arpLanesEdited()
{
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
}

void PhoqerAudioProcessor::loadArpLanes(const juce::String& text)
{
    const auto lanes = parseLanes(text);
    for (size_t i = 0; i < lanes.size(); ++i) arpLanes[i].store(packLane(lanes[i]), std::memory_order_relaxed);
    parameters.state.setProperty("arpLanes", formatLanes(lanes), nullptr);
}

juce::String PhoqerAudioProcessor::saveArpLanes() const
{
    std::array<phoqer::ArpLaneStep, phoqer::arpLaneSteps> lanes {};
    for (int i = 0; i < phoqer::arpLaneSteps; ++i) lanes[static_cast<size_t>(i)] = getArpLane(i);
    return formatLanes(lanes);
}

juce::String PhoqerAudioProcessor::canonicalArpLanes(const juce::String& text)
{
    return formatLanes(parseLanes(text));
}

void PhoqerAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    auto saved = parameters.copyState();
    juce::String selection;
    {
        const juce::ScopedLock sl(pendingLock);
        if (selectionPending) selection = pendingSelection;     // restored but not applied yet: save what was restored
    }
    saved.setProperty("loadedVoiceSelection", selection.isNotEmpty() ? selection : voices.captureVoiceSelection(), nullptr);
    if (auto xml = saved.createXml())
        copyXmlToBinary(*xml, destinationData);
}

void PhoqerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            // Older sessions predate these parameters. Restore their compatible defaults,
            // including when an old state replaces a currently edited new instance.
            for (const auto* id : { "standaloneTempo", "glide", "wailBend", "wailHold", "metronomeEnabled", "metronomeVolume", "metronomeBeats", "metronomeAccent",
                                  "metronomeSound", "countIn", "yap", "nitro", "nitroNote1", "nitroNote2", "nitroNote3", "nitroLevel1",
                                  "nitroLevel2", "nitroLevel3", "nitroWidth", "nitroStrum", "nitroDetune",
                                  "arp", "arpRate", "arpPattern", "arpHold", "arpGate", "arpSwing", "arpOctaves" })
                if (! restored.getChildWithProperty("id", id).isValid())
                {
                    auto* p = parameters.getParameter(id);
                    juce::ValueTree child("PARAM");
                    child.setProperty("id", id, nullptr);
                    child.setProperty("value", p->convertFrom0to1(p->getDefaultValue()), nullptr);
                    restored.addChild(child, -1, nullptr);
                }
            parameters.replaceState(restored);
            loadArpLanes(restored.getProperty("arpLanes").toString());     // older songs have none: all full
            // The standalone reopens at no more than 0 dB OUTPUT, so a loud last session can't blast on start.
            // Lower settings are kept; a DAW session restores exactly what it saved.
            if (wrapperType == wrapperType_Standalone)
                if (auto* out = parameters.getParameter("output"); out != nullptr && out->convertFrom0to1(out->getValue()) > 0.0f)
                    out->setValueNotifyingHost(out->convertTo0to1(0.0f));
            const auto selection = restored.getProperty("loadedVoiceSelection").toString();
            if (juce::MessageManager::getInstanceWithoutCreating() == nullptr || juce::MessageManager::existsAndIsCurrentThread())
            {
                { const juce::ScopedLock sl(pendingLock); selectionPending = false; }
                voices.restoreVoiceSelection(selection);
            }
            else
            {
                { const juce::ScopedLock sl(pendingLock); pendingSelection = selection; selectionPending = true; }
                triggerAsyncUpdate();
            }
        }
}

void PhoqerAudioProcessor::handleAsyncUpdate()
{
    juce::String selection;
    {
        const juce::ScopedLock sl(pendingLock);
        if (! selectionPending) return;
        selection = pendingSelection;
        selectionPending = false;
    }
    voices.restoreVoiceSelection(selection);
}

juce::AudioProcessorEditor* PhoqerAudioProcessor::createEditor()
{
    return new phoqer::ui::PhoqerEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhoqerAudioProcessor();
}

