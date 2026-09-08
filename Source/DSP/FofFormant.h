#pragma once

#include "FofGrain.h"
#include "../Core/CharacterPreset.h"

#include <array>
#include <cmath>

namespace phoqer
{
// One formant band, built as a collapsed-tail FOF bank.
//
// Grains are triggered once per fundamental period and overlap heavily: GROAN's
// 34 Hz bandwidth decays over about 66 ms, so at 640 Hz roughly 43 grains are
// sounding at once. Holding 43 independent oscillators per band would be
// unaffordable, and recycling a fixed pool would overwrite still-loud grains and
// click.
//
// Neither is necessary. Once a grain leaves its attack skirt it is a pure
// exponential sharing this band's pole, and the recursion is linear, so every
// tail-phase grain sums into a single accumulator. Only grains still inside the
// skirt need their own state, and that is a handful even at the top of the
// keyboard. The tail is a recursion rather than a buffer, so it is never
// truncated at all.
class FofFormant
{
public:
    static constexpr int attackSlots = 8;

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        tail.reset();
        for (auto& grain : attack)
        {
            grain.busy = false;
            grain.age = 0;
            grain.carrier.reset();
            grain.skirt.reset();
        }
    }

    void trigger(const FormantSpec& spec, double periodSamples, double skirtSeconds,
                 float amplitude) noexcept
    {
        const auto nyquist = sampleRate * 0.5;
        // Clamp bandwidth, never Q. A narrow bandwidth is simply a long decay.
        const auto centre = clamp(20.0, nyquist * 0.94, static_cast<double>(spec.frequency));
        const auto bandwidth = clamp(4.0, nyquist * 0.70, static_cast<double>(spec.bandwidth));

        const auto radius = std::exp(-pi<double> * bandwidth / sampleRate);
        const auto omega = twoPi<double> * centre / sampleRate;

        // Overlapping grains sum to a / (1 - r^period) in steady state, so
        // scaling by (1 - r^period) holds loudness constant as pitch and
        // bandwidth change.
        const auto normalisation = 1.0 - std::pow(radius, periodSamples);

        auto riseSamples = static_cast<int>(skirtSeconds * sampleRate);
        if (riseSamples < 1)
            riseSamples = 1;

        const auto gain = amplitude * spec.gain * static_cast<float>(normalisation);
        tail.setPole(radius, omega);

        auto slot = -1;
        for (int index = 0; index < attackSlots; ++index)
            if (! attack[static_cast<size_t>(index)].busy)
            {
                slot = index;
                break;
            }

        if (slot < 0)
        {
            // Every slot busy: retire the oldest grain by folding its current,
            // skirted state into the tail. The output is continuous across that
            // sample; the grain simply stops widening and carries on decaying.
            auto oldest = 0;
            for (int index = 1; index < attackSlots; ++index)
                if (attack[static_cast<size_t>(index)].age > attack[static_cast<size_t>(oldest)].age)
                    oldest = index;
            auto& victim = attack[static_cast<size_t>(oldest)];
            tail.addScaled(victim.skirtValue(), victim.carrier);
            victim.busy = false;
            slot = oldest;
        }

        attack[static_cast<size_t>(slot)].start(radius, omega, pi<double> / riseSamples,
                                                riseSamples, gain);
    }

    float process() noexcept
    {
        // Advance the tail first, so a grain retiring this sample is not
        // advanced twice on its retirement sample.
        tail.advance();
        auto sum = tail.imaginary;

        for (auto& grain : attack)
        {
            if (! grain.busy)
                continue;

            grain.carrier.advance();
            grain.skirt.advance();
            ++grain.age;

            if (grain.age >= grain.riseSamples)
            {
                sum += grain.carrier.imaginary;      // the skirt has reached 1
                tail.addScaled(1.0f, grain.carrier);
                grain.busy = false;
            }
            else
            {
                sum += grain.skirtValue() * grain.carrier.imaginary;
            }
        }

        if (! std::isfinite(sum))
        {
            reset();
            return 0.0f;
        }
        return sum;
    }

    bool isQuiescent() const noexcept
    {
        if (! tail.isQuiescent())
            return false;
        for (const auto& grain : attack)
            if (grain.busy)
                return false;
        return true;
    }

private:
    ComplexResonator tail;
    std::array<AttackGrain, attackSlots> attack {};
    double sampleRate = 44100.0;
};
}
