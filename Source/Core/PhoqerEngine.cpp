#include "PhoqerEngine.h"

namespace phoqer
{
juce::SynthesiserVoice* PhoqerSynthesiser::findVoiceToSteal(juce::SynthesiserSound* sound,
                                                             int, int) const
{
    SealVoice* bestReleased = nullptr;
    SealVoice* bestActive = nullptr;
    float releasedLevel = std::numeric_limits<float>::max();
    float activePriority = std::numeric_limits<float>::max();
    uint64_t newestAge = 0;

    for (auto* baseVoice : voices)
        if (auto* voice = dynamic_cast<SealVoice*>(baseVoice);
            voice != nullptr && voice->canPlaySound(sound))
            newestAge = juce::jmax(newestAge, voice->getAge());

    for (auto* baseVoice : voices)
    {
        auto* voice = dynamic_cast<SealVoice*>(baseVoice);
        if (voice == nullptr || ! voice->canPlaySound(sound))
            continue;

        const auto level = voice->getEnvelopeLevel();
        if (voice->isReleasing())
        {
            if (level < releasedLevel || (level == releasedLevel
                                          && (bestReleased == nullptr || voice->getAge() < bestReleased->getAge())))
            {
                bestReleased = voice;
                releasedLevel = level;
            }
            continue;
        }

        const auto recencyProtection = 0.04f / (1.0f + static_cast<float>(newestAge - voice->getAge()));
        const auto priority = level + recencyProtection;
        if (priority < activePriority || (priority == activePriority
                                          && (bestActive == nullptr || voice->getAge() < bestActive->getAge())))
        {
            bestActive = voice;
            activePriority = priority;
        }
    }

    return bestReleased != nullptr ? bestReleased : bestActive;
}

PhoqerEngine::PhoqerEngine(juce::AudioProcessorValueTreeState& state)
    : parameters(state)
{
    boomParameter = parameters.getRawParameterValue(parameters::boom);
    airParameter = parameters.getRawParameterValue(parameters::air);
    barkParameter = parameters.getRawParameterValue(parameters::bark);
    vowelParameter = parameters.getRawParameterValue(parameters::vowel);
    spaceParameter = parameters.getRawParameterValue(parameters::space);
    tideParameter = parameters.getRawParameterValue(parameters::tide);
    outputParameter = parameters.getRawParameterValue(parameters::output);
    jassert(boomParameter != nullptr && airParameter != nullptr && barkParameter != nullptr
            && vowelParameter != nullptr && spaceParameter != nullptr && tideParameter != nullptr
            && outputParameter != nullptr);

    for (int voice = 0; voice < voiceCount; ++voice)
        synth.addVoice(new SealVoice(0x91e10da5u + static_cast<uint32_t>(voice) * 0x9e3779b9u,
                                     ageCounter));
    synth.addSound(new SealSound());
    synth.setNoteStealingEnabled(true);
}

void PhoqerEngine::prepare(double sampleRate, int maximumBlockSize, int numChannels)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    for (int voice = 0; voice < synth.getNumVoices(); ++voice)
        if (auto* sealVoice = dynamic_cast<SealVoice*>(synth.getVoice(voice)))
            sealVoice->prepare(sampleRate, maximumBlockSize);

    digitalStage.prepare(sampleRate, numChannels);
    spaceStage.prepare(sampleRate, maximumBlockSize);
    outputStage.prepare(sampleRate);
    waveformDecimation = juce::jmax(1, static_cast<int>(sampleRate / 1000.0));
    waveformCountdown = 0;
    reset();
}

void PhoqerEngine::reset()
{
    synth.allNotesOff(0, false);
    digitalStage.reset();
    spaceStage.reset();
    outputStage.reset();
    telemetry.publishFace({});
    telemetry.publishMeter(0.0f, 0.0f);
}

MacroState PhoqerEngine::readMacros() const noexcept
{
    return {
        juce::jlimit(0.0f, 1.0f, boomParameter->load(std::memory_order_relaxed)),
        juce::jlimit(0.0f, 1.0f, airParameter->load(std::memory_order_relaxed)),
        juce::jlimit(0.0f, 1.0f, barkParameter->load(std::memory_order_relaxed)),
        juce::jlimit(0.0f, 1.0f, vowelParameter->load(std::memory_order_relaxed)),
        juce::jlimit(0.0f, 1.0f, spaceParameter->load(std::memory_order_relaxed)),
        juce::jlimit(0.0f, 1.0f, tideParameter->load(std::memory_order_relaxed))
    };
}

void PhoqerEngine::process(juce::AudioBuffer<float>& output, juce::MidiBuffer& midi)
{
    output.clear();
    const auto macros = readMacros();
    for (int voice = 0; voice < synth.getNumVoices(); ++voice)
        if (auto* sealVoice = dynamic_cast<SealVoice*>(synth.getVoice(voice)))
            sealVoice->setMacros(macros);

    synth.renderNextBlock(output, midi, 0, output.getNumSamples());
    digitalStage.process(output);
    spaceStage.process(output, macros.space);
    outputStage.setOutputDb(outputParameter->load(std::memory_order_relaxed));
    outputStage.process(output);
    publishTelemetry(output);
}

void PhoqerEngine::publishTelemetry(const juce::AudioBuffer<float>& output) noexcept
{
    const SealVoice* dominant = nullptr;
    float bestScore = 0.0f;
    for (int voice = 0; voice < synth.getNumVoices(); ++voice)
    {
        const auto* sealVoice = dynamic_cast<const SealVoice*>(synth.getVoice(voice));
        if (sealVoice == nullptr)
            continue;
        const auto& state = sealVoice->getTelemetry();
        if (! state.active)
            continue;
        const auto recency = 1.0f + 0.05f / (1.0f + static_cast<float>(ageCounter - state.age));
        const auto score = state.envelope * (0.4f + 0.6f * state.velocity) * recency;
        if (score > bestScore)
        {
            dominant = sealVoice;
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
        face.jawWidth = juce::jlimit(0.0f, 1.0f,
            0.10f + 0.56f * vocal.mouthOpen + 0.20f * (1.0f - vocal.mouthRound)
            + 0.22f * vocal.barkTransient);
        face.throatTension = vocal.throatPressure;
        face.eyeSquint = juce::jlimit(0.0f, 1.0f, vocal.barkTransient * 0.9f);
        face.eyeOpen = juce::jlimit(0.0f, 1.0f,
            0.18f * vocal.callIntensity + 0.58f * vocal.pitchLift
            + 0.24f * state.registerPosition);
        face.headLift = juce::jlimit(0.0f, 1.0f,
            0.34f * state.registerPosition + 0.50f * vocal.pitchLift
            + 0.16f * vocal.callPhase);
        face.intensity = juce::jlimit(0.0f, 1.0f, vocal.callIntensity
                                               + 0.1f * outputStage.getRms());
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
