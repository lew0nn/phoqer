#pragma once

#include "../Core/Random.h"
#include <algorithm>
#include <cmath>

namespace phoqer
{
class SmoothedRandom
{
public:
    void prepare(double newSampleRate, float updateRateHz, Random* source) noexcept
    {
        random = source;
        samplesPerTarget = static_cast<int>(std::max(1.0, newSampleRate / updateRateHz));
        reset();
    }

    void reset() noexcept { current = target = increment = 0.0f; countdown = 1; }

    float next() noexcept
    {
        if (--countdown <= 0)
        {
            target = random != nullptr ? random->bipolar() : 0.0f;
            countdown = samplesPerTarget;
            increment = (target - current) / static_cast<float>(samplesPerTarget);
        }
        current += increment;
        return current;
    }

private:
    Random* random = nullptr;
    int samplesPerTarget = 1, countdown = 1;
    float current = 0.0f, target = 0.0f, increment = 0.0f;
};
}
