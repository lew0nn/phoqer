#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/DspPrimitives.h"

namespace phoqer
{
struct MetronomeSettings
{
    float volume = 0.7f;        // 0..1
    int beatsPerBar = 4;
    bool accent = true;         // beat 1 of each bar higher and louder
    int sound = 0;              // 0 BEEP, 1 WOOD, 2 SEAL
};

// Standalone monitoring click. Mix after recording so WAV takes contain only the instrument.
class Metronome
{
public:
    void prepare(double rate) noexcept
    {
        sampleRate = std::max(1.0, rate);
        reset();
    }

    void reset() noexcept
    {
        running = false;
        beatPhase = 0.0;
        clickLength = clickSample = 2;
    }

    // Start again from beat 1 on the next block, e.g. for a count-in (audio thread).
    void restart() noexcept { running = false; }

    void process(AudioBuffer& buffer, bool enabled, float bpm, const MetronomeSettings& settings) noexcept
    {
        if (enabled && ! running) { beatPhase = 1.0; beats = 0.0; clicks = 0; } // Click immediately when enabled.
        running = enabled;
        if (! enabled && clickSample >= clickLength) return;
        const double step = clamp(20.0f, 400.0f, bpm) / (60.0 * sampleRate);
        const double level = 0.2 * settings.volume * settings.volume;     // 70% matches the original click
        constexpr float ceiling = 0.9661f;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            if (enabled)
            {
                if (beatPhase >= 1.0)
                {
                    beatPhase -= std::floor(beatPhase);
                    startClick(settings);
                }
                beatPhase += step; // Tempo changes preserve the fraction of the current beat.
                beats += step;
            }
            if (clickSample >= clickLength) continue;
            const float click = static_cast<float>(level * (accented ? 1.25 : 1.0) * nextClickSample());
            ++clickSample; // Turning off lets this short click finish without a discontinuity.

            float peak = 0.0f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                peak = std::max(peak, std::abs(buffer.getSample(channel, sample)));
            // Reserve headroom only where needed; apply the same gain to both channels.
            const float gain = std::min(1.0f, (ceiling - std::abs(click)) / std::max(ceiling - std::abs(click), peak));
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain + click);
        }
    }

    // Beats since it was switched on: the first click is beat 0, the next beat 1, and so on.
    double getBeats() const noexcept { return beats; }

private:
    void startClick(const MetronomeSettings& settings) noexcept
    {
        accented = settings.accent && clicks % std::max(1, settings.beatsPerBar) == 0;
        ++clicks;
        sound = clamp(0, 2, settings.sound);
        constexpr double lengths[] { 0.015, 0.030, 0.050 };
        clickLength = std::max(2, static_cast<int>(std::round(lengths[sound] * sampleRate)));
        clickSample = 0;
        phase = 0.0;
    }

    double nextClickSample() noexcept
    {
        const double t = clickSample / sampleRate, position = static_cast<double>(clickSample) / (clickLength - 1);
        const double hump = std::sin(pi<double> * position);
        switch (sound)
        {
            case 1:     // WOOD: an instant knock that rings down fast, two inharmonic partials
            {
                const double f = accented ? 1250.0 : 1000.0;
                const double envelope = std::exp(-t / 0.006) * std::min(1.0, t / 0.0005) * (1.0 - position);
                return 0.7 * envelope * (std::sin(twoPi<double> * f * t) + 0.45 * std::sin(twoPi<double> * 2.71 * f * t));
            }
            case 2:     // SEAL: a short boop that drops in pitch
            {
                const double f = (accented ? 1.35 : 1.0) * (520.0 + 480.0 * std::exp(-t / 0.012));
                phase += twoPi<double> * f / sampleRate;
                return hump * hump * std::sin(phase);
            }
            default:    // BEEP
                return hump * hump * std::sin(twoPi<double> * (accented ? 2400.0 : 1800.0) * t);
        }
    }

    double sampleRate = 48000.0, beatPhase = 0.0, beats = 0.0, phase = 0.0;
    int clickLength = 2, clickSample = 2, sound = 0;
    long long clicks = 0;
    bool running = false, accented = false;
};
}
