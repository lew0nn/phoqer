#pragma once

#include "../Core/CharacterPreset.h"
#include "../Core/DspPrimitives.h"
#include "../Core/Random.h"

namespace phoqer
{
// The pitch-synchronous trigger that drives the FOF grains. Fires once per
// fundamental period and carries that period's amplitude.
//
// This is where BURP's growl lives. The reference recording alternates between a
// clean regime and a period-doubled one every 100 to 300 ms, which at an 80 Hz
// fundamental is roughly every 8 to 24 periods, so a per-period switch
// probability of about 0.06 reproduces it without a scripted sequence. While
// doubled, every other period is attenuated, which puts real energy at f0/2.
class GlottalPulse
{
public:
    struct Tick
    {
        bool trigger = false;
        float amplitude = 1.0f;
        double periodSamples = 0.0;
    };

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        phase = 1.0;          // fire on the first sample of a note
        alternate = false;
        doubling = false;
    }

    Tick advance(double frequencyHz, const CharacterPreset& preset, Random& random) noexcept
    {
        Tick tick;
        const auto safeFrequency = clamp(20.0, sampleRate * 0.45, frequencyHz);
        const auto periodSamples = sampleRate / safeFrequency;

        phase += 1.0 / periodSamples;
        if (phase < 1.0)
            return tick;

        phase -= 1.0;
        if (phase >= 1.0)       // a very high note should not queue up triggers
            phase = 0.0;

        const auto jitter = 1.0 + static_cast<double>(preset.jitter) * random.bipolar();
        tick.periodSamples = periodSamples * clamp(0.5, 1.5, jitter);

        if (preset.subharmonicChance > 0.0f && random.nextFloat() < preset.subharmonicChance)
            doubling = ! doubling;

        auto amplitude = 1.0f;
        if (doubling)
        {
            alternate = ! alternate;
            if (alternate)
                amplitude *= 0.35f;
        }
        else
        {
            alternate = false;
        }

        amplitude *= 1.0f + preset.shimmer * random.bipolar();
        tick.amplitude = clamp(0.0f, 2.0f, amplitude);
        tick.trigger = true;
        return tick;
    }

    bool isDoubling() const noexcept { return doubling; }

private:
    double sampleRate = 44100.0;
    double phase = 1.0;
    bool alternate = false;
    bool doubling = false;
};
}
