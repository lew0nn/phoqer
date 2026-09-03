#pragma once

#include <array>

namespace phoqer::parameters
{
enum Index
{
    boom = 0,
    air,
    bark,
    vowel,
    space,
    tide,
    detune,
    output,
    character,
    count
};

struct Descriptor
{
    const char* id;
    const char* name;
    float defaultValue;
    float minimum;
    float maximum;
    float step;
    const char* unit;
};

inline constexpr std::array<Descriptor, count> descriptors {{
    { "boom",   "BOOM",   0.50f,   0.0f, 1.0f, 0.001f, "" },
    { "air",    "AIR",    0.25f,   0.0f, 1.0f, 0.001f, "" },
    { "bark",   "BARK",   0.35f,   0.0f, 1.0f, 0.001f, "" },
    { "vowel",  "VOWEL",  0.35f,   0.0f, 1.0f, 0.001f, "" },
    { "space",  "REVERB", 0.00f,   0.0f, 1.0f, 0.001f, "" },
    { "tide",   "TIDE",   0.25f,   0.0f, 1.0f, 0.001f, "" },
    { "detune", "DETUNE", 0.00f,   0.0f, 1.0f, 0.001f, "" },
    { "output",    "OUTPUT",    0.00f, -24.0f, 18.0f, 0.010f, "dB" },
    { "character", "CHARACTER", 1.00f,   0.0f,  2.0f, 1.000f, "" }
}};
}
