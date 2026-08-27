#pragma once

#include <cstdint>

namespace phoqer
{
class Random
{
public:
    explicit Random(uint32_t seed = 0x6d2b79f5u) noexcept : state(seed != 0 ? seed : 1u) {}

    uint32_t nextUInt() noexcept
    {
        auto x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    float nextFloat() noexcept { return static_cast<float>(nextUInt() >> 8) * (1.0f / 16777216.0f); }
    float bipolar() noexcept { return 2.0f * nextFloat() - 1.0f; }
    float range(float minimum, float maximum) noexcept { return minimum + (maximum - minimum) * nextFloat(); }

private:
    uint32_t state;
};
}

