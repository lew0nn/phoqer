#pragma once

#include <cstdint>

namespace phoqer
{
// The three sound characters. The numeric order is load-bearing: it matches the
// JUCE AudioParameterChoice order in PluginProcessor.cpp and the UI colour
// themes (0 red, 1 purple, 2 ice). Do not reorder.
enum class SealCharacter : uint8_t
{
    burp = 0,   // bass, red
    squeal,     // default, purple
    groan,      // high, ice
    count
};

inline constexpr SealCharacter defaultSealCharacter = SealCharacter::squeal;

constexpr bool isKnownSealCharacter(SealCharacter character) noexcept
{
    return static_cast<uint8_t>(character) < static_cast<uint8_t>(SealCharacter::count);
}
}
