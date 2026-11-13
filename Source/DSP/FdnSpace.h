#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/DspPrimitives.h"
#include <array>
#include <vector>

namespace phoqer
{
// REVERB (SPACE): an eight-line feedback delay network. Stereo in through a short pre-delay and early
// reflections, two all-pass diffusers per side, then eight lines mixed by an orthogonal Hadamard matrix
// (lossless, so the decay is set only by each line's own gain). Four lines drift by a fraction of a
// millisecond at slow, unrelated rates, which keeps a long tail from ringing metallic. Each line decays
// per band: the lows for the full time, the highs sooner, so a big space is also a dark one. SPACE runs
// from a short room (about 0.9 s) to a dark cave (about 4 s), as before.
class FdnSpace
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void process(AudioBuffer& buffer, float spaceAmount) noexcept;

private:
    static constexpr int lineCount = 8;
    struct Delay
    {
        std::vector<float> data;
        int write = 0;
        void clear() noexcept { std::fill(data.begin(), data.end(), 0.0f); write = 0; }
        float at(int samplesAgo) const noexcept
        {
            auto i = write - samplesAgo;
            while (i < 0) i += static_cast<int>(data.size());
            return data[static_cast<size_t>(i)];
        }
        float atFractional(float samplesAgo) const noexcept
        {
            const auto whole = static_cast<int>(samplesAgo);
            const auto frac = samplesAgo - static_cast<float>(whole);
            return at(whole) + frac * (at(whole + 1) - at(whole));
        }
        void push(float v) noexcept
        {
            data[static_cast<size_t>(write)] = v;
            if (++write >= static_cast<int>(data.size())) write = 0;
        }
    };
    struct AllPass
    {
        Delay delay;
        int length = 1;
        float run(float x) noexcept
        {
            const auto delayed = delay.at(length);
            const auto v = x + 0.6f * delayed;
            delay.push(v);
            return delayed - 0.6f * v;
        }
    };

    double rate = 44100.0;
    Delay preLeft, preRight;
    std::array<AllPass, 2> diffuseLeft, diffuseRight;
    std::array<Delay, lineCount> lines;
    std::array<float, lineCount> length {};      // samples
    std::array<float, lineCount> lowState {};    // each line's band split
    std::array<double, lineCount> modPhase {};
    std::array<double, lineCount> modStep {};
    float modDepth = 0.0f;                       // samples
    float lastAmount = -1.0f;
    float preDelay = 0.0f;
    float wetTrim = 1.0f;
    std::array<float, lineCount> gainLow {}, gainHigh {};
    float splitCoefficient = 0.0f;
    std::array<int, 6> earlyLeft {}, earlyRight {};
    LinearSmoother wet;
    bool resting = false;          // SPACE 0 and silent: cleared, not computing (process)
    long long silentRun = 0;       // samples of silent input in a row
    long long restAfter = 0;       // silence needed before resting: everything in the pre-delay and diffusers gone
    float tailPeak = 1.0f;         // the loudest line output in the last block
};
}
