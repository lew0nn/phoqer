#pragma once

#include "../Core/DspPrimitives.h"

#include <cmath>

namespace phoqer
{
// A complex rotation with a decaying radius. Advancing it is four multiplies and
// two adds, and Im(z) traces exp(-alpha*t) * sin(omega*t), which is exactly the
// impulse response of a formant resonance.
//
// Q never appears as a coefficient. The only pole parameter is the radius
// r = exp(-pi*BW/fs), which is below 1 for any positive bandwidth, so the
// resonator is unconditionally stable at any Q. That is what lets GROAN use
// Q 33 and BURP use Q 53, where the old feedback filter bank had to clamp Q to 8.
struct ComplexResonator
{
    float real = 0.0f, imaginary = 0.0f;
    float poleReal = 0.0f, poleImaginary = 0.0f;

    void setPole(double radius, double omega) noexcept
    {
        poleReal = static_cast<float>(radius * std::cos(omega));
        poleImaginary = static_cast<float>(radius * std::sin(omega));
    }

    void advance() noexcept
    {
        const auto nextReal = real * poleReal - imaginary * poleImaginary;
        const auto nextImaginary = real * poleImaginary + imaginary * poleReal;
        real = nextReal;
        imaginary = nextImaginary;
        // Explicit flush to zero. The offline harness has no ScopedNoDenormals,
        // and a long sustained release would otherwise crawl through denormals.
        if (std::abs(real) + std::abs(imaginary) < 1.0e-20f)
            real = imaginary = 0.0f;
    }

    void addScaled(float gain, const ComplexResonator& source) noexcept
    {
        real += gain * source.real;
        imaginary += gain * source.imaginary;
    }

    void reset() noexcept { real = imaginary = 0.0f; }
    bool isQuiescent() const noexcept { return real == 0.0f && imaginary == 0.0f; }
};

// A grain that is still inside its raised-cosine attack skirt. Once the skirt
// completes, the grain is a pure exponential sharing its band's pole, so it is
// folded into the band's single tail accumulator and its slot is freed.
struct AttackGrain
{
    ComplexResonator carrier;   // decaying, amplitude and phase baked in
    ComplexResonator skirt;     // undamped rotator, radius exactly 1
    int age = 0;
    int riseSamples = 1;
    bool busy = false;

    // offset: how far, in samples (0 to 1), the grain already is past its start: both rotations
    // begin that far along, so a cycle that began between two samples is heard where it began.
    void start(double radius, double omega, double skirtRadians,
               int newRiseSamples, float amplitude, double offset = 0.0) noexcept
    {
        carrier.setPole(radius, omega);
        const auto decayed = static_cast<double>(amplitude) * std::pow(radius, offset);
        carrier.real = static_cast<float>(decayed * std::cos(omega * offset));
        carrier.imaginary = static_cast<float>(decayed * std::sin(omega * offset));
        skirt.setPole(1.0, skirtRadians);
        skirt.real = static_cast<float>(std::cos(skirtRadians * offset));
        skirt.imaginary = static_cast<float>(std::sin(skirtRadians * offset));
        riseSamples = newRiseSamples > 0 ? newRiseSamples : 1;
        age = 0;
        busy = true;
    }

    // Raised cosine from 0 to 1 across the rise: 0.5 * (1 - cos(pi * age / rise)).
    float skirtValue() const noexcept { return 0.5f * (1.0f - skirt.real); }
};
}
