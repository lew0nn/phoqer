#include "FdnSpace.h"

#include <algorithm>
#include <cmath>

namespace phoqer
{
namespace
{
// Line lengths in ms: no two share a factor, so their echoes never line up into a pitch.
constexpr std::array<double, 8> lineMs { 31.7, 37.1, 41.3, 47.9, 53.3, 59.9, 67.1, 73.3 };
// Slow, unrelated drift on every other line (Hz); the rest stay put.
constexpr std::array<double, 8> driftHz { 0.13, 0.0, 0.17, 0.0, 0.23, 0.0, 0.29, 0.0 };
constexpr std::array<double, 6> earlyLeftMs { 7.1, 11.9, 17.3, 23.9, 29.1, 36.7 };
constexpr std::array<double, 6> earlyRightMs { 8.3, 13.1, 19.7, 21.7, 31.3, 34.9 };
constexpr std::array<float, 6> earlyGain { 0.42f, 0.33f, 0.27f, 0.21f, 0.16f, 0.12f };
// Wet level against the old CheapSpace at the same SPACE (measured, phrase renders at 0.2, 0.5 and 1.0).
constexpr float wetScale = 0.63f;

void hadamard(std::array<float, 8>& v) noexcept     // fast Walsh-Hadamard, scaled to stay lossless
{
    for (int span = 1; span < 8; span *= 2)
        for (int i = 0; i < 8; i += 2 * span)
            for (int j = i; j < i + span; ++j)
            {
                const auto a = v[static_cast<size_t>(j)], b = v[static_cast<size_t>(j + span)];
                v[static_cast<size_t>(j)] = a + b;
                v[static_cast<size_t>(j + span)] = a - b;
            }
    for (auto& x : v) x *= 0.35355339f;     // 1 / sqrt(8)
}
}

void FdnSpace::prepare(double sampleRate, int)
{
    rate = sampleRate;
    const auto samples = [&](double ms) { return static_cast<int>(std::lround(ms * 0.001 * rate)); };
    for (auto* d : { &preLeft, &preRight })
        d->data.assign(static_cast<size_t>(samples(60.0) + 4), 0.0f);
    const double diffuseMs[2][2] { { 3.1, 1.3 }, { 3.7, 1.7 } };
    for (int k = 0; k < 2; ++k)
    {
        auto& l = diffuseLeft[static_cast<size_t>(k)];
        auto& r = diffuseRight[static_cast<size_t>(k)];
        l.length = std::max(1, samples(diffuseMs[0][k]));
        r.length = std::max(1, samples(diffuseMs[1][k]));
        l.delay.data.assign(static_cast<size_t>(l.length + 2), 0.0f);
        r.delay.data.assign(static_cast<size_t>(r.length + 2), 0.0f);
    }
    modDepth = static_cast<float>(0.00045 * rate);
    for (size_t i = 0; i < lines.size(); ++i)
    {
        length[i] = static_cast<float>(samples(lineMs[i]));
        lines[i].data.assign(static_cast<size_t>(length[i] + modDepth + 4.0f), 0.0f);
        modStep[i] = driftHz[i] / rate;
        modPhase[i] = 0.25 * static_cast<double>(i);
    }
    for (size_t k = 0; k < 6; ++k)
    {
        earlyLeft[k] = samples(earlyLeftMs[k]);
        earlyRight[k] = samples(earlyRightMs[k]);
    }
    wet.reset(sampleRate, 0.05);
    wet.setCurrentAndTargetValue(0.0f);
    lastAmount = -1.0f;
    restAfter = static_cast<long long>(preLeft.data.size());
    for (const auto& a : diffuseLeft) restAfter += static_cast<long long>(a.delay.data.size());
    resting = false;
    silentRun = 0;
    tailPeak = 1.0f;
    reset();
}

void FdnSpace::reset() noexcept
{
    preLeft.clear();
    preRight.clear();
    for (auto& a : diffuseLeft) a.delay.clear();
    for (auto& a : diffuseRight) a.delay.clear();
    for (auto& l : lines) l.clear();
    lowState.fill(0.0f);
}

void FdnSpace::process(AudioBuffer& buffer, float spaceAmount) noexcept
{
    const auto amount = clamp(0.0f, 1.0f, spaceAmount);
    wet.setTargetValue(amount * 0.55f);
    if (amount != lastAmount)       // decay times and colour follow SPACE
    {
        lastAmount = amount;
        const double lowSeconds = 0.9 + 3.1 * amount;                 // a short room to a cave
        const double highSeconds = lowSeconds * (0.6 - 0.3 * amount); // the highs die sooner, more so as it grows
        for (size_t i = 0; i < lines.size(); ++i)
        {
            gainLow[i] = static_cast<float>(std::pow(10.0, -3.0 * length[i] / (lowSeconds * rate)));
            gainHigh[i] = static_cast<float>(std::pow(10.0, -3.0 * length[i] / (highSeconds * rate)));
        }
        splitCoefficient = static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265358979 * (3000.0 - 1800.0 * amount) / rate));
        preDelay = static_cast<float>((0.004 + 0.022 * amount) * rate);
        // matched to the old reverb's level: 1.2 dB less up to half way, easing to none at the top
        wetTrim = amount < 0.5f ? 0.874f : 0.874f + 0.252f * (amount - 0.5f);
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;
    const int n = buffer.getNumSamples();
    // At SPACE 0 the reverb isn't heard. Once the input has been silent long enough for everything in it to
    // die away (its tail below -180 dB), it rests, cleared, so silence costs nothing; sound or SPACE wakes it at
    // once. Its wobble keeps moving while it rests, so waking up is the same as having run all along.
    bool silentIn = true;
    for (int s = 0; s < n && silentIn; ++s) silentIn = left[s] == 0.0f && right[s] == 0.0f;
    silentRun = silentIn ? silentRun + n : 0;
    if (amount <= 0.0f && wet.getCurrentValue() <= 0.0f && silentIn
        && (resting || (tailPeak < 1.0e-9f && silentRun > restAfter)))
    {
        if (! resting) { reset(); resting = true; }
        for (size_t i = 0; i < lines.size(); ++i)
            if (modStep[i] > 0.0) { modPhase[i] += modStep[i] * n; modPhase[i] -= std::floor(modPhase[i]); }
        return;
    }
    resting = false;
    float blockTail = 0.0f;
    const auto pre = std::max(1, static_cast<int>(preDelay));
    for (int s = 0; s < n; ++s)
    {
        const auto inLeft = left[s], inRight = right[s];
        preLeft.push(inLeft);
        preRight.push(inRight);

        float earlyL = 0.0f, earlyR = 0.0f;
        for (size_t k = 0; k < 6; ++k)
        {
            earlyL += earlyGain[k] * preLeft.at(earlyLeft[k]);
            earlyR += earlyGain[k] * preRight.at(earlyRight[k]);
        }
        auto feedL = preLeft.at(pre), feedR = preRight.at(pre);
        for (auto& a : diffuseLeft) feedL = a.run(feedL);
        for (auto& a : diffuseRight) feedR = a.run(feedR);

        std::array<float, 8> taps {};
        for (size_t i = 0; i < lines.size(); ++i)
        {
            float y;
            if (modStep[i] > 0.0)
            {
                modPhase[i] += modStep[i];
                modPhase[i] -= std::floor(modPhase[i]);
                y = lines[i].atFractional(length[i] - modDepth * (0.5f + 0.5f * fastSin2Pi(modPhase[i])));
            }
            else
                y = lines[i].at(static_cast<int>(length[i]));
            taps[i] = y;
            blockTail = std::max(blockTail, std::abs(y));
        }
        // Each line's decay per band: lows by gainLow, highs by gainHigh.
        std::array<float, 8> back {};
        for (size_t i = 0; i < lines.size(); ++i)
        {
            lowState[i] += splitCoefficient * (taps[i] - lowState[i]);
            back[i] = gainLow[i] * lowState[i] + gainHigh[i] * (taps[i] - lowState[i]);
        }
        hadamard(back);
        for (size_t i = 0; i < lines.size(); ++i)
            lines[i].push(back[i] + 0.35f * (i % 2 == 0 ? feedL : feedR));

        const auto mix = wet.getNextValue();
        const auto dryGain = 1.0f - 0.30f * mix;
        const auto wetGain = wetScale * wetTrim * mix;
        const auto tailL = 0.5f * (taps[0] - taps[2] + taps[4] - taps[6]);
        const auto tailR = 0.5f * (taps[1] - taps[3] + taps[5] - taps[7]);
        left[s] = inLeft * dryGain + (tailL + 0.6f * earlyL) * wetGain;
        if (right != left)
            right[s] = inRight * dryGain + (tailR + 0.6f * earlyR) * wetGain;
    }
    tailPeak = blockTail;
}
}
