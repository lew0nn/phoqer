#pragma once

#include "../Core/PhoqerEngine.h"
#include "../Voice/VoicePersonality.h"

namespace phoqer
{
// Offline development helper. Never call this from processBlock.
struct CoreSanity
{
    struct Result
    {
        bool silenceIsSilent = false;
        bool eightVoicesFinite = false;
        bool extremesBounded = false;
        bool formantSweepFinite = false;
        bool vowelAnchorsDistinct = false;
        bool callEvolutionCoherent = false;
        bool extremeCombinationsDistinct = false;
        bool repeatedNotesVary = false;
        bool reprepareFinite = false;
        bool personalitiesVary = false;
        bool telemetryNormalized = false;

        bool passed() const noexcept
        {
            return silenceIsSilent && eightVoicesFinite && extremesBounded && formantSweepFinite
                && vowelAnchorsDistinct && callEvolutionCoherent && extremeCombinationsDistinct
                && repeatedNotesVary && reprepareFinite && personalitiesVary && telemetryNormalized;
        }
    };

    static bool finiteAndBounded(const juce::AudioBuffer<float>& buffer, float limit = 1.25f) noexcept
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto value = buffer.getSample(channel, sample);
                if (! std::isfinite(value) || std::abs(value) > limit)
                    return false;
            }
        return true;
    }

    static bool faceIsNormalized(const FaceTelemetry& face) noexcept
    {
        const float values[] { face.mouthOpen, face.mouthRound, face.jawWidth, face.throatTension,
                               face.eyeSquint, face.eyeOpen, face.headLift, face.intensity };
        for (const auto value : values)
            if (! std::isfinite(value) || value < 0.0f || value > 1.0f)
                return false;
        return true;
    }

    static Result run(PhoqerEngine& engine, juce::AudioProcessorValueTreeState& state)
    {
        constexpr int blockSize = 512;
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        Result result;

        engine.prepare(44100.0, blockSize, 2);
        buffer.clear();
        engine.process(buffer, midi);
        result.silenceIsSilent = buffer.getMagnitude(0, blockSize) == 0.0f;

        for (int note = 0; note < voiceCount; ++note)
            midi.addEvent(juce::MidiMessage::noteOn(1, 48 + note * 2, 0.8f), 0);
        engine.process(buffer, midi);
        result.eightVoicesFinite = finiteAndBounded(buffer) && buffer.getMagnitude(0, blockSize) > 1.0e-6f;

        const char* ids[] { parameters::boom, parameters::air, parameters::bark,
                            parameters::vowel, parameters::tide };
        for (const auto* id : ids)
            state.getRawParameterValue(id)->store(1.0f);
        midi.clear();
        engine.process(buffer, midi);
        result.extremesBounded = finiteAndBounded(buffer);

        FormantBank bank;
        bank.prepare(48000.0);
        MacroState sweepMacros { 1.0f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f };
        VoicePersonality sweepPersonality;
        VocalState sweepVocal;
        result.formantSweepFinite = true;
        for (int sample = 0; sample < 4096; ++sample)
        {
            sweepVocal.mouthOpen = static_cast<float>(sample) / 4095.0f;
            sweepVocal.mouthRound = 1.0f - sweepVocal.mouthOpen;
            sweepVocal.vowelMorph = sweepVocal.mouthOpen;
            sweepVocal.callPhase = sweepVocal.mouthOpen;
            const auto input = sample == 0 ? 1.0f : 0.03f * std::sin(sample * 0.17f);
            const auto value = bank.process(input, 60.0f, sweepMacros, sweepVocal,
                                            sweepPersonality, std::sin(sample * 0.013f));
            result.formantSweepFinite = result.formantSweepFinite
                                     && std::isfinite(value) && std::abs(value) <= 2.01f;
        }

        result.vowelAnchorsDistinct = true;
        auto previousAnchor = FormantBank::describeVowel(0.0f);
        for (int anchor = 1; anchor < 5; ++anchor)
        {
            const auto currentAnchor = FormantBank::describeVowel(static_cast<float>(anchor) / 4.0f);
            float frequencyDistance = 0.0f;
            float shapeDistance = 0.0f;
            for (size_t formant = 0; formant < 4; ++formant)
            {
                frequencyDistance += std::abs(currentAnchor.frequency[formant]
                                            - previousAnchor.frequency[formant]);
                shapeDistance += std::abs(currentAnchor.q[formant] - previousAnchor.q[formant])
                               + 10.0f * std::abs(currentAnchor.gain[formant]
                                               - previousAnchor.gain[formant]);
            }
            result.vowelAnchorsDistinct = result.vowelAnchorsDistinct
                                       && frequencyDistance > 450.0f && shapeDistance > 1.0f;
            previousAnchor = currentAnchor;
        }

        BehaviourEngine behaviour;
        VoicePersonality evolutionPersonality;
        evolutionPersonality.vowelTravel = 0.30f;
        MacroState evolutionMacros { 0.5f, 0.4f, 0.35f, 0.35f, 0.0f, 1.0f };
        behaviour.start(0.8f, evolutionMacros, evolutionPersonality);
        float minimumVowel = 1.0f, maximumVowel = 0.0f;
        float minimumMouth = 1.0f, maximumMouth = 0.0f;
        float previousPhase = 0.0f;
        bool phaseMonotonic = true;
        for (int sample = 0; sample < 96000; ++sample)
        {
            if (sample == 72000)
                behaviour.noteOff();
            const auto vocal = behaviour.process(1.0f / 48000.0f, evolutionMacros, 0.8f,
                                                 std::sin(sample * 0.0007f));
            minimumVowel = juce::jmin(minimumVowel, vocal.vowelMorph);
            maximumVowel = juce::jmax(maximumVowel, vocal.vowelMorph);
            minimumMouth = juce::jmin(minimumMouth, vocal.mouthOpen);
            maximumMouth = juce::jmax(maximumMouth, vocal.mouthOpen);
            phaseMonotonic = phaseMonotonic && vocal.callPhase + 1.0e-6f >= previousPhase;
            previousPhase = vocal.callPhase;
        }
        result.callEvolutionCoherent = phaseMonotonic && previousPhase > 0.99f
                                    && maximumVowel - minimumVowel > 0.16f
                                    && maximumMouth - minimumMouth > 0.35f;

        struct RenderFeatures
        {
            double rms = 0.0;
            double variation = 0.0;
            double lateToEarly = 0.0;
            bool finite = true;
        };

        const auto renderFeatures = [&engine, &state](const MacroState& settings)
        {
            state.getRawParameterValue(parameters::boom)->store(settings.boom);
            state.getRawParameterValue(parameters::air)->store(settings.air);
            state.getRawParameterValue(parameters::bark)->store(settings.bark);
            state.getRawParameterValue(parameters::vowel)->store(settings.vowel);
            state.getRawParameterValue(parameters::space)->store(settings.space);
            state.getRawParameterValue(parameters::tide)->store(settings.tide);
            state.getRawParameterValue(parameters::output)->store(0.0f);

            constexpr int renderBlockSize = 256;
            constexpr int renderBlocks = 128;
            engine.prepare(48000.0, renderBlockSize, 2);
            juce::AudioBuffer<float> renderBuffer(2, renderBlockSize);
            juce::MidiBuffer renderMidi;
            RenderFeatures features;
            double sumSquares = 0.0, earlyEnergy = 0.0, lateEnergy = 0.0;
            float previousSample = 0.0f;
            int sampleCount = 0;
            for (int block = 0; block < renderBlocks; ++block)
            {
                renderMidi.clear();
                if (block == 0)
                    renderMidi.addEvent(juce::MidiMessage::noteOn(1, 55, 0.82f), 0);
                engine.process(renderBuffer, renderMidi);
                features.finite = features.finite && finiteAndBounded(renderBuffer);
                for (int sample = 0; sample < renderBlockSize; ++sample)
                {
                    const auto value = renderBuffer.getSample(0, sample);
                    const auto square = static_cast<double>(value) * value;
                    sumSquares += square;
                    features.variation += std::abs(value - previousSample);
                    previousSample = value;
                    if (block < renderBlocks / 3)
                        earlyEnergy += square;
                    if (block >= renderBlocks * 2 / 3)
                        lateEnergy += square;
                    ++sampleCount;
                }
            }
            features.rms = std::sqrt(sumSquares / juce::jmax(1, sampleCount));
            features.variation /= juce::jmax(1, sampleCount);
            features.lateToEarly = std::sqrt((lateEnergy + 1.0e-12) / (earlyEnergy + 1.0e-12));
            return features;
        };

        const std::array<MacroState, 4> contrastSettings {{
            { 1.0f, 0.0f, 0.0f, 0.00f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.25f, 0.0f, 0.3f },
            { 0.5f, 0.4f, 1.0f, 0.50f, 0.0f, 0.2f },
            { 0.7f, 0.3f, 0.4f, 0.75f, 0.0f, 1.0f }
        }};
        std::array<RenderFeatures, 4> contrastFeatures {};
        for (size_t setting = 0; setting < contrastSettings.size(); ++setting)
            contrastFeatures[setting] = renderFeatures(contrastSettings[setting]);

        result.extremeCombinationsDistinct = true;
        for (size_t setting = 0; setting < contrastFeatures.size(); ++setting)
        {
            result.extremeCombinationsDistinct = result.extremeCombinationsDistinct
                                               && contrastFeatures[setting].finite
                                               && contrastFeatures[setting].rms > 1.0e-5;
            if (setting == 0)
                continue;
            const auto& previous = contrastFeatures[setting - 1];
            const auto& current = contrastFeatures[setting];
            const auto distance = std::abs(std::log((current.rms + 1.0e-9)
                                                  / (previous.rms + 1.0e-9)))
                                + std::abs(std::log((current.variation + 1.0e-9)
                                                  / (previous.variation + 1.0e-9)))
                                + std::abs(current.lateToEarly - previous.lateToEarly);
            result.extremeCombinationsDistinct = result.extremeCombinationsDistinct
                                               && distance > 0.08;
        }

        const MacroState repeatSettings { 0.55f, 0.35f, 0.4f, 0.35f, 0.0f, 0.65f };
        const auto firstRepeat = renderFeatures(repeatSettings);
        const auto secondRepeat = renderFeatures(repeatSettings);
        const auto repeatDistance = std::abs(firstRepeat.rms - secondRepeat.rms)
                                  + std::abs(firstRepeat.variation - secondRepeat.variation)
                                  + 0.1 * std::abs(firstRepeat.lateToEarly - secondRepeat.lateToEarly);
        result.repeatedNotesVary = firstRepeat.finite && secondRepeat.finite
                                && repeatDistance > 1.0e-4;

        result.reprepareFinite = true;
        for (const auto rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            engine.prepare(rate, 127, 2);
            juce::AudioBuffer<float> rateBuffer(2, 127);
            juce::MidiBuffer rateMidi;
            rateMidi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
            engine.process(rateBuffer, rateMidi);
            result.reprepareFinite = result.reprepareFinite && finiteAndBounded(rateBuffer);
        }

        Random random(1234u);
        const auto first = VoicePersonality::create(random);
        const auto second = VoicePersonality::create(random);
        result.personalitiesVary = first.pitchOvershoot != second.pitchOvershoot
                                && first.formantScale != second.formantScale;
        result.telemetryNormalized = faceIsNormalized(engine.getTelemetry().readFace());
        return result;
    }
};
}
