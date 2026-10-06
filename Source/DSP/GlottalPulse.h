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
        periodScale = 1.0;
        alternate = false;
        doubling = false;
    }

    // growl scales the preset's jitter and doubling (BARK); 1 leaves the preset as measured.
    Tick advance(double frequencyHz, const CharacterPreset& preset, Random& random,
                 float growl = 1.0f, float extraDoubling = 0.0f) noexcept
    {
        Tick tick;
        const auto safeFrequency = clamp(20.0, sampleRate * 0.45, frequencyHz);
        const auto periodSamples = sampleRate / safeFrequency;

        // The phase runs at this period's own length, so jitter really moves
        // the pitch from one period to the next instead of only the gain.
        phase += 1.0 / (periodSamples * periodScale);
        if (phase < 1.0)
            return tick;

        phase -= 1.0;
        if (phase >= 1.0)       // a very high note should not queue up triggers
            phase = 0.0;

        periodScale = clamp(0.5, 1.5, 1.0 + static_cast<double>(preset.jitter * growl) * random.bipolar());
        tick.periodSamples = periodSamples * periodScale;

        const auto doublingChance = preset.subharmonicChance * growl + extraDoubling;
        if (doublingChance > 0.0f && random.nextFloat() < doublingChance)
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
    double periodScale = 1.0;   // this period's length against the nominal one
    bool alternate = false;
    bool doubling = false;
};
}
