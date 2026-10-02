#pragma once

#include <JuceHeader.h>

namespace phoqer::testui
{
// The PHOQER app logo as native-resolution pixel art in the voice's palette. Draw with
// nearest-neighbour scaling. Mirrors Tools/branding/phoqer_branding.py, which exports the same
// artwork as PNGs (including the .exe icon); keep the two in step.

// The app icon at small sizes (up to 32 px): a framed tile with the round seal-sun face on a calm
// sea and soft whiskers. Designed on a 32 x 32 grid; eye highlights appear from 32 px up. (The large
// sizes use a separate drawing with neck and shoulders, exported by the branding script.)
// 'framed' adds the voice-coloured frame the .exe icon has; the header shows it without.
juce::Image renderAppIcon(int character, int size, bool framed = true);

// Slanted "Outrun" PHOQER wordmark with flat chrome bands and speed lines, cropped to its pixels.
juce::Image renderOutrunWordmark(int character, bool onDarkBackground);
}
