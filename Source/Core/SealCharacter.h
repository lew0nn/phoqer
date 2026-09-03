#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace phoqer
{
enum class SealCharacter : uint8_t
{
    lowBurp = 0,
    barkGroan,
    padShout,
    count
};

struct SealCharacterDescriptor
{
    SealCharacter character;
    const char* id;
    const char* developmentName;
    bool implemented;
};

inline constexpr SealCharacter defaultSealCharacter = SealCharacter::barkGroan;

inline constexpr std::array<SealCharacterDescriptor,
                            static_cast<size_t>(SealCharacter::count)> sealCharacters {{
    { SealCharacter::lowBurp,   "low-burp",   "Low / Burping",      false },
    { SealCharacter::barkGroan, "bark-groan", "Main / Bark-Groan",  true  },
    { SealCharacter::padShout,  "pad-shout",  "Pad / Moan-Shout",   false }
}};

constexpr bool isKnownSealCharacter(SealCharacter character) noexcept
{
    return static_cast<uint8_t>(character) < static_cast<uint8_t>(SealCharacter::count);
}

constexpr bool isImplementedSealCharacter(SealCharacter character) noexcept
{
    return isKnownSealCharacter(character)
        && sealCharacters[static_cast<size_t>(character)].implemented;
}
}
