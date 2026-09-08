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
        bool characterRoutingValid = false;
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
        bool behaviorModesDistinct = false;
        bool charactersDistinct = false;
        bool sustainHolds = false;

        bool passed() const noexcept
        {
            return silenceIsSilent && characterRoutingValid && eightVoicesFinite
                && extremesBounded && formantSweepFinite
                && vowelAnchorsDistinct && callEvolutionCoherent && extremeCombinationsDistinct
                && repeatedNotesVary && reprepareFinite && personalitiesVary && telemetryNormalized
                && behaviorModesDistinct && charactersDistinct && sustainHolds;
        }
    };

    static bool finiteAndBounded(const AudioBuffer& buffer, float limit = 16.0f) noexcept
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

    static Result run(PhoqerEngine& engine)
    {
        constexpr int blockSize = 512;
        AudioBuffer buffer(2, blockSize);
        MacroState macros;
        Result result;

        engine.prepare(44100.0, blockSize, 2);
        buffer.clear();
        engine.process(buffer, nullptr, 0, macros, 0.0f);
        result.silenceIsSilent = buffer.getMagnitude(0, blockSize) == 0.0f;

        // Every character must speak. GROAN swells in over 280 ms, so give each
        // one time to reach level rather than judging a single block.
        const MidiEvent characterNote { MidiEventType::noteOn, 0, 0, 55, 0.8f };
        bool everyCharacterAudible = true;
        for (const auto character : { SealCharacter::burp, SealCharacter::squeal,
                                      SealCharacter::groan })
        {
            MacroState characterMacros;
            characterMacros.character = character;
            float peak = 0.0f;
            for (int block = 0; block < 64; ++block)
            {
                engine.process(buffer, block == 0 ? &characterNote : nullptr,
                               block == 0 ? 1 : 0, characterMacros, 0.0f);
                const auto magnitude = buffer.getMagnitude(0, blockSize);
                if (magnitude > peak)
                    peak = magnitude;
            }
            everyCharacterAudible = everyCharacterAudible
                && engine.getActiveCharacter() == character
                && peak > 0.01f;
        }
        result.characterRoutingValid = everyCharacterAudible;

        std::array<MidiEvent, voiceCount> noteOns {};
        for (int note = 0; note < voiceCount; ++note)
            noteOns[static_cast<size_t>(note)] = { MidiEventType::noteOn, 0, 0, 48 + note * 2, 0.8f };
        engine.process(buffer, noteOns.data(), static_cast<int>(noteOns.size()), macros, 0.0f);
        result.eightVoicesFinite = finiteAndBounded(buffer) && buffer.getMagnitude(0, blockSize) > 1.0e-6f;

        macros = { 1.0f, 1.0f, 1.0f, 1.0f, macros.space, 1.0f };
        engine.process(buffer, nullptr, 0, macros, 0.0f);
        result.extremesBounded = finiteAndBounded(buffer);

        // Stress the FOF band across its whole range, including a 4 Hz bandwidth
        // that the old feedback filter bank could never have run.
        FofFormant sweepFormant;
        sweepFormant.prepare(48000.0);
        result.formantSweepFinite = true;
        for (int sample = 0; sample < 48000; ++sample)
        {
            if (sample % 80 == 0)
            {
                const auto position = static_cast<float>(sample) / 48000.0f;
                FormantSpec spec;
                spec.frequency = lerp(position, 120.0f, 9000.0f);
                spec.bandwidth = lerp(position, 4.0f, 900.0f);
                spec.gain = 1.0f;
                sweepFormant.trigger(spec, 80.0, 0.0015, 1.0f);
            }
            const auto value = sweepFormant.process();
            result.formantSweepFinite = result.formantSweepFinite
                                     && std::isfinite(value) && std::abs(value) <= 8.0f;
        }

        // The character presets must stay genuinely different, and the two
        // narrow formants that define GROAN and BURP must survive any edit.
        {
            const auto& burpPreset = characterPreset(SealCharacter::burp);
            const auto& squealPreset = characterPreset(SealCharacter::squeal);
            const auto& groanPreset = characterPreset(SealCharacter::groan);
            // Band 1 is the formant that defines each character: GROAN's narrow
            // singing formant, and BURP's very narrow second formant at band 2.
            const auto groanQ = groanPreset.formants[1].frequency
                              / groanPreset.formants[1].bandwidth;
            const auto burpQ = burpPreset.formants[2].frequency
                             / burpPreset.formants[2].bandwidth;
            result.vowelAnchorsDistinct = groanQ > 25.0f && burpQ > 40.0f;

            const CharacterPreset* presets[3] { &burpPreset, &squealPreset, &groanPreset };
            for (int first = 0; first < 3; ++first)
                for (int second = first + 1; second < 3; ++second)
                {
                    float distance = 0.0f;
                    for (size_t formant = 0; formant < formantBandCount; ++formant)
                        distance += std::abs(presets[first]->formants[formant].frequency
                                           - presets[second]->formants[formant].frequency);
                    result.vowelAnchorsDistinct = result.vowelAnchorsDistinct
                                               && distance > 200.0f;
                }
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
            minimumVowel = std::min(minimumVowel, vocal.vowelMorph);
            maximumVowel = std::max(maximumVowel, vocal.vowelMorph);
            minimumMouth = std::min(minimumMouth, vocal.mouthOpen);
            maximumMouth = std::max(maximumMouth, vocal.mouthOpen);
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

        const auto renderFeatures = [&engine](const MacroState& settings)
        {
            constexpr int renderBlockSize = 256;
            // Cover the full finite call rather than measuring only its attack.
            constexpr int renderBlocks = 384;
            engine.prepare(48000.0, renderBlockSize, 2);
            AudioBuffer renderBuffer(2, renderBlockSize);
            RenderFeatures features;
            double sumSquares = 0.0, earlyEnergy = 0.0, lateEnergy = 0.0;
            float previousSample = 0.0f;
            int sampleCount = 0;
            for (int block = 0; block < renderBlocks; ++block)
            {
                const MidiEvent noteOn { MidiEventType::noteOn, 0, 0, 55, 0.82f };
                engine.process(renderBuffer, block == 0 ? &noteOn : nullptr, block == 0 ? 1 : 0,
                               settings, 0.0f);
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
            features.rms = std::sqrt(sumSquares / std::max(1, sampleCount));
            features.variation /= std::max(1, sampleCount);
            features.lateToEarly = std::sqrt((lateEnergy + 1.0e-12) / (earlyEnergy + 1.0e-12));
            return features;
        };

        std::array<RenderFeatures, 5> behaviorFeatures {};
        for (int mode = 0; mode < 5; ++mode)
        {
            MacroState settings;
            settings.behaviorMode = static_cast<BehaviourMode>(mode);
            behaviorFeatures[static_cast<size_t>(mode)] = renderFeatures(settings);
        }
        result.behaviorModesDistinct = true;
        for (size_t mode = 0; mode < behaviorFeatures.size(); ++mode)
        {
            result.behaviorModesDistinct = result.behaviorModesDistinct
                                         && behaviorFeatures[mode].finite
                                         && behaviorFeatures[mode].rms > 1.0e-5;
            if (mode == 0) continue;
            const auto& firstMode = behaviorFeatures[0];
            const auto& currentMode = behaviorFeatures[mode];
            const auto distance = std::abs(std::log((currentMode.rms + 1.0e-9)
                                                  / (firstMode.rms + 1.0e-9)))
                                + std::abs(std::log((currentMode.variation + 1.0e-9)
                                                  / (firstMode.variation + 1.0e-9)))
                                + std::abs(currentMode.lateToEarly - firstMode.lateToEarly);
            result.behaviorModesDistinct = result.behaviorModesDistinct && distance > 0.02;
        }

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
                                               && distance > 0.015;
        }

        std::array<RenderFeatures, 3> characterFeatures {};
        for (size_t index = 0; index < characterFeatures.size(); ++index)
        {
            MacroState settings;
            settings.character = static_cast<SealCharacter>(index);
            characterFeatures[index] = renderFeatures(settings);
        }
        result.charactersDistinct = true;
        for (size_t index = 0; index < characterFeatures.size(); ++index)
        {
            result.charactersDistinct = result.charactersDistinct
                                      && characterFeatures[index].finite
                                      && characterFeatures[index].rms > 1.0e-4;
            for (size_t other = index + 1; other < characterFeatures.size(); ++other)
            {
                const auto& earlier = characterFeatures[index];
                const auto& later = characterFeatures[other];
                const auto distance = std::abs(std::log((later.rms + 1.0e-9)
                                                      / (earlier.rms + 1.0e-9)))
                                    + std::abs(std::log((later.variation + 1.0e-9)
                                                      / (earlier.variation + 1.0e-9)))
                                    + std::abs(later.lateToEarly - earlier.lateToEarly);
                result.charactersDistinct = result.charactersDistinct && distance > 0.05;
            }
        }

        const MacroState repeatSettings { 0.55f, 0.35f, 0.4f, 0.35f, 0.0f, 0.65f };
        const auto firstRepeat = renderFeatures(repeatSettings);
        const auto secondRepeat = renderFeatures(repeatSettings);
        const auto repeatDistance = std::abs(firstRepeat.rms - secondRepeat.rms)
                                  + std::abs(firstRepeat.variation - secondRepeat.variation)
                                  + 0.1 * std::abs(firstRepeat.lateToEarly - secondRepeat.lateToEarly);
        result.repeatedNotesVary = firstRepeat.finite && secondRepeat.finite
                                && repeatDistance > 1.0e-7;

        result.reprepareFinite = true;
        for (const auto rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            engine.prepare(rate, 127, 2);
            AudioBuffer rateBuffer(2, 127);
            const MidiEvent rateNote { MidiEventType::noteOn, 0, 0, 60, 1.0f };
            engine.process(rateBuffer, &rateNote, 1, MacroState {}, 0.0f);
            result.reprepareFinite = result.reprepareFinite && finiteAndBounded(rateBuffer);
        }

        Random random(1234u);
        const auto first = VoicePersonality::create(random);
        const auto second = VoicePersonality::create(random);
        result.personalitiesVary = first.airiness != second.airiness
                                && first.barkVariation != second.barkVariation
                                && first.pitchDrift == 0.0f && second.pitchDrift == 0.0f
                                && first.formantScale == 1.0f && second.formantScale == 1.0f;
        // A held key must still sound after 9 s and must stop after release.
        {
            constexpr int sustainBlockSize = 512;
            constexpr int blocksPerSecond = 48000 / sustainBlockSize;
            engine.prepare(48000.0, sustainBlockSize, 2);
            AudioBuffer sustainBuffer(2, sustainBlockSize);
            const MidiEvent held { MidiEventType::noteOn, 0, 0, 55, 0.85f };
            float lateMagnitude = 0.0f;
            for (int block = 0; block < blocksPerSecond * 10; ++block)
            {
                engine.process(sustainBuffer, block == 0 ? &held : nullptr,
                               block == 0 ? 1 : 0, MacroState {}, 0.0f);
                if (block >= blocksPerSecond * 9)
                {
                    const auto magnitude = sustainBuffer.getMagnitude(0, sustainBlockSize);
                    if (magnitude > lateMagnitude)
                        lateMagnitude = magnitude;
                }
            }
            const MidiEvent lift { MidiEventType::noteOff, 0, 0, 55, 0.0f };
            float tailMagnitude = 0.0f;
            for (int block = 0; block < blocksPerSecond * 2; ++block)
            {
                engine.process(sustainBuffer, block == 0 ? &lift : nullptr,
                               block == 0 ? 1 : 0, MacroState {}, 0.0f);
                if (block >= blocksPerSecond)
                {
                    const auto magnitude = sustainBuffer.getMagnitude(0, sustainBlockSize);
                    if (magnitude > tailMagnitude)
                        tailMagnitude = magnitude;
                }
            }
            result.sustainHolds = lateMagnitude > 0.01f && tailMagnitude < 1.0e-4f;
        }

        result.telemetryNormalized = faceIsNormalized(engine.getTelemetry().readFace());
        return result;
    }
};
}
