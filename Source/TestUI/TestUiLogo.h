#pragma once

#include <JuceHeader.h>

namespace phoqer::testui
{
// The PHOQER app logo as native-resolution pixel art in the voice's palette. Draw with
// nearest-neighbour scaling. Mirrors Tools/branding/phoqer_branding.py, which exports the same
// artwork as PNGs (including the .exe icon); keep the two in step.

// "Seal sun": the striped synthwave sun is the seal's face. Designed on a 32 x 32 grid; any size
// works, eye highlights and whiskers appear from 32 px up.
juce::Image renderSealSunIcon(int character, int size);

// Slanted "Outrun" PHOQER wordmark with flat chrome bands and speed lines, cropped to its pixels.
juce::Image renderOutrunWordmark(int character, bool onDarkBackground);
}
