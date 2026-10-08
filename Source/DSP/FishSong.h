#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/FishSprite.h"

#include <array>
#include <cmath>

namespace phoqer
{
// What the fed seal sings: the fish itself, written into the sound. The 40 rows of the sprite are the
// first 40 harmonics of one low sung note (top row highest), so the song is a single deep, buzzing seal
// voice, and since harmonics are evenly spaced it still draws the fish on a spectrogram, head first,
// below about 3.3 kHz. Each feed sings a different note. Silent unless started.
class FishSong
{
public:
    static constexpr int rows = fishHeight * 4, columns = fishWidth * 2;
    static constexpr double seconds = 2.4;

    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (0.004 * sampleRate)));    // about 4 ms: no clicks
        reset();
    }

    void reset() noexcept
    {
        position = -1;
        level.fill(0.0f);
        phase = 0.0;
    }

    // noteIndex picks the sung note, A1 to E2, wrapping.
    void start(double delaySeconds, int noteIndex) noexcept
    {
        static constexpr double notes[] { 55.0, 61.74, 65.41, 73.42, 82.41, 61.74 };
        f0 = notes[static_cast<size_t>(((noteIndex % 6) + 6) % 6)];
        lowest = 1;     // from the note itself: a full voice, not a whistle
        position = -static_cast<long>(delaySeconds * sampleRate);
        phase = 0.0;
    }

    bool isPlaying() const noexcept { return position != -1; }

    void process(AudioBuffer& output) noexcept
    {
        if (position == -1)
            return;
        const long length = static_cast<long>(seconds * sampleRate);
        const double twoPi = 6.283185307179586;
        for (int i = 0; i < output.getNumSamples(); ++i, ++position)
        {
            if (position < 0) continue;
            if (position >= length + static_cast<long>(0.05 * sampleRate)) { reset(); return; }
            const int column = position < length ? static_cast<int>(position * columns / length) : -1;
            // a singer's vibrato, 5 Hz, a third of a percent: alive, and too small to smear the picture
            const double t = static_cast<double>(position) / sampleRate;
            phase += twoPi * f0 * (1.0 + 0.0035 * std::sin(twoPi * 5.0 * t)) / sampleRate;
            if (phase > twoPi * 1024.0) phase -= twoPi * 1024.0;
            // sin((n-1)x) = 2 cos x sin(nx) - sin((n+1)x), walking down from the top harmonic
            const double c2 = 2.0 * std::cos(phase);
            const int top = lowest + rows - 1;
            double above = std::sin((top + 1) * phase), current = std::sin(top * phase);
            float sum = 0.0f;
            for (int r = 0; r < rows; ++r)      // top row = highest harmonic
            {
                const auto k = static_cast<size_t>(r);
                const float target = column < 0 ? 0.0f : brightness(fishStraight[r / 4][column / 2]);
                level[k] += smoothing * (target - level[k]);
                sum += level[k] * static_cast<float>(current);
                const double below = c2 * current - above;
                above = current;
                current = below;
            }
            for (int channel = 0; channel < output.getNumChannels(); ++channel)
                output.addSample(channel, i, sum * gain);
        }
    }

private:
    // Louder is brighter on a spectrogram: the outline and pupil loudest, the eye's white silent.
    static float brightness(char c) noexcept
    {
        switch (c)
        {
            case 'X': case 'K': return 1.0f;
            case 'd': return 0.7f;
            case 'o': return 0.5f;
            case 's': return 0.3f;
            default:  return 0.0f;
        }
    }

    static constexpr float gain = 0.016f;
    double sampleRate = 48000.0, f0 = 65.41, phase = 0.0;
    int lowest = 1;
    long position = -1;
    float smoothing = 0.01f;
    std::array<float, rows> level {};
};
}
