#include "PhoqerEngine.h"
#include "DspPrimitives.h"

#include <algorithm>
#include <limits>

namespace phoqer
{
PhoqerEngine::PhoqerEngine()
    : voices {{
        { 0x91e10da5u + 0u * 0x9e3779b9u, ageCounter },
        { 0x91e10da5u + 1u * 0x9e3779b9u, ageCounter },
        { 0x91e10da5u + 2u * 0x9e3779b9u, ageCounter },
        { 0x91e10da5u + 3u * 0x9e3779b9u, ageCounter },
        { 0x91e10da5u + 4u * 0x9e3779b9u, ageCounter },
        { 0x91e10da5u + 5u * 0x9e3779b9u, ageCounter },
        { 0x91e10da5u + 6u * 0x9e3779b9u, ageCounter },
        { 0x91e10da5u + 7u * 0x9e3779b9u, ageCounter }
    }}
{
    pitchWheels.fill(8192);
}

void PhoqerEngine::prepare(double sampleRate, int maximumBlockSize, int)
{
    for (auto& voice : voices)
        voice.prepare(sampleRate, maximumBlockSize);

    spaceStage.prepare(sampleRate, maximumBlockSize);
    outputStage.prepare(sampleRate);
    waveformDecimation = std::max(1, static_cast<int>(sampleRate / 1000.0));
    waveformCountdown = 0;
    reset();
}

void PhoqerEngine::reset()
{
    for (auto& voice : voices)
        voice.hardReset();
    pitchWheels.fill(8192);
    spaceStage.reset();
    outputStage.reset();
    activeCharacter = defaultSealCharacter;
    telemetry.publishFace({});
    telemetry.publishMeter(0.0f, 0.0f);
}

SealVoice* PhoqerEngine::findVoiceForNoteOn() noexcept
{
    for (auto& voice : voices)
        if (! voice.isActive())
            return &voice;

    SealVoice* bestReleased = nullptr;
    SealVoice* bestActive = nullptr;
    float releasedLevel = std::numeric_limits<float>::max();
    float activePriority = std::numeric_limits<float>::max();
    uint64_t newestAge = 0;

    for (const auto& voice : voices)
        newestAge = std::max(newestAge, voice.getAge());

    for (auto& voice : voices)
    {
        const auto level = voice.getEnvelopeLevel();
        if (voice.isReleasing())
        {
            if (level < releasedLevel || (level == releasedLevel
                                          && (bestReleased == nullptr || voice.getAge() < bestReleased->getAge())))
            {
                bestReleased = &voice;
                releasedLevel = level;
            }
            continue;
        }

        const auto recencyProtection = 0.04f / (1.0f + static_cast<float>(newestAge - voice.getAge()));
        const auto priority = level + recencyProtection;
        if (priority < activePriority || (priority == activePriority
                                          && (bestActive == nullptr || voice.getAge() < bestActive->getAge())))
        {
            bestActive = &voice;
            activePriority = priority;
        }
    }

    return bestReleased != nullptr ? bestReleased : bestActive;
}

void PhoqerEngine::handleEvent(const MidiEvent& event) noexcept
{
    const auto channel = clamp(0, 15, event.channel);
    switch (event.type)
    {
        case MidiEventType::noteOn:
        {
            if (event.value <= 0.0f)
            {
                for (auto& voice : voices)
                    if (voice.matchesNote(channel, event.note))
                        voice.stopNote(true);
                break;
            }

            if (auto* voice = findVoiceForNoteOn())
            {
                if (voice->isActive())
                    voice->stopNote(false);
                voice->startNote(channel, clamp(0, 127, event.note),
                                 clamp(0.0f, 1.0f, event.value), pitchWheels[static_cast<size_t>(channel)]);
            }
            break;
        }
        case MidiEventType::noteOff:
            for (auto& voice : voices)
                if (voice.matchesNote(channel, event.note))
                    voice.stopNote(true);
            break;
        case MidiEventType::pitchWheel:
        {
            const auto value = clamp(-1.0f, 1.0f, event.value);
            const auto wheel = clamp(0, 16383, static_cast<int>(8192.0f + value * 8192.0f));
            pitchWheels[static_cast<size_t>(channel)] = wheel;
            for (auto& voice : voices)
                if (voice.matchesChannel(channel))
                    voice.pitchWheelMoved(wheel);
            break;
        }
        case MidiEventType::allNotesOff:
            for (auto& voice : voices)
                if (voice.matchesChannel(channel))
                    voice.stopNote(true);
            break;
    }
}

void PhoqerEngine::renderVoices(AudioBuffer& output, int startSample, int numSamples)
{
    if (numSamples <= 0)
        return;
    for (auto& voice : voices)
        voice.renderNextBlock(output, startSample, numSamples);
}

void PhoqerEngine::process(AudioBuffer& output, const MidiEvent* events, int eventCount,
                           const MacroState& inputMacros, float outputDecibels)
{
    output.clear();
    const auto requestedCharacter = isKnownSealCharacter(inputMacros.character)
        ? inputMacros.character
        : defaultSealCharacter;
    if (requestedCharacter != activeCharacter)
    {
        for (auto& voice : voices)
            voice.hardReset();
        spaceStage.reset();
        activeCharacter = requestedCharacter;
    }

    // Character slots without an approved synthesis design remain deliberately
    // silent. They must not masquerade as the completed main character.
    if (! isImplementedSealCharacter(activeCharacter))
    {
        telemetry.publishFace({});
        telemetry.publishMeter(0.0f, 0.0f);
        return;
    }

    const MacroState macros {
        clamp(0.0f, 1.0f, inputMacros.boom),
        clamp(0.0f, 1.0f, inputMacros.air),
        clamp(0.0f, 1.0f, inputMacros.bark),
        clamp(0.0f, 1.0f, inputMacros.vowel),
        clamp(0.0f, 1.0f, inputMacros.space),
        clamp(0.0f, 1.0f, inputMacros.tide),
        clamp(0.0f, 1.0f, inputMacros.detune),
        activeCharacter
    };
    for (auto& voice : voices)
        voice.setMacros(macros);

    int cursor = 0;
    for (int index = 0; index < eventCount; ++index)
    {
        const auto eventSample = clamp(cursor, output.getNumSamples(), events[index].sampleOffset);
        renderVoices(output, cursor, eventSample - cursor);
        handleEvent(events[index]);
        cursor = eventSample;
    }
    renderVoices(output, cursor, output.getNumSamples() - cursor);

    spaceStage.process(output, macros.space);
    outputStage.setOutputDb(outputDecibels);
    outputStage.process(output);
    publishTelemetry(output);
}

void PhoqerEngine::publishTelemetry(const AudioBuffer& output) noexcept
{
    const SealVoice* dominant = nullptr;
    float bestScore = 0.0f;
    for (const auto& voice : voices)
    {
        const auto& state = voice.getTelemetry();
        if (! state.active)
            continue;
        const auto recency = 1.0f + 0.05f / (1.0f + static_cast<float>(ageCounter - state.age));
        const auto score = state.envelope * (0.4f + 0.6f * state.velocity) * recency;
        if (score > bestScore)
        {
            dominant = &voice;
            bestScore = score;
        }
    }

    FaceTelemetry face;
    if (dominant != nullptr)
    {
        const auto& state = dominant->getTelemetry();
        const auto& vocal = state.vocal;
        face.mouthOpen = vocal.mouthOpen;
        face.mouthRound = vocal.mouthRound;
        face.jawWidth = clamp(0.0f, 1.0f,
            0.10f + 0.56f * vocal.mouthOpen + 0.20f * (1.0f - vocal.mouthRound)
            + 0.22f * vocal.barkTransient);
        face.throatTension = vocal.throatPressure;
        face.eyeSquint = clamp(0.0f, 1.0f, vocal.barkTransient * 0.9f);
        face.eyeOpen = clamp(0.0f, 1.0f,
            0.18f * vocal.callIntensity + 0.58f * vocal.pitchLift + 0.24f * state.registerPosition);
        face.headLift = clamp(0.0f, 1.0f,
            0.34f * state.registerPosition + 0.50f * vocal.pitchLift + 0.16f * vocal.callPhase);
        face.intensity = clamp(0.0f, 1.0f, vocal.callIntensity + 0.1f * outputStage.getRms());
    }
    telemetry.publishFace(face);
    telemetry.publishMeter(outputStage.getPeak(), outputStage.getRms());

    if (output.getNumChannels() == 0)
        return;
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
    {
        if (--waveformCountdown <= 0)
        {
            const auto mono = output.getNumChannels() > 1
                ? 0.5f * (output.getSample(0, sample) + output.getSample(1, sample))
                : output.getSample(0, sample);
            telemetry.pushWaveform(mono);
            waveformCountdown = waveformDecimation;
        }
    }
}
}
