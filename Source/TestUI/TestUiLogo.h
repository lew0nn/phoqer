#pragma once

#include <JuceHeader.h>

namespace phoqer::testui
{
// PHOQER's logo art. The logo and the app icon are PNGs drawn by the branding scripts and built in;
// the Outrun wordmark (the boot splash) is drawn here as native-resolution pixel art in the voice's
// palette. Draw everything with nearest-neighbour scaling.

// An image built into the plugin, by its original file name (an empty image if missing).
juce::Image loadAssetImage(const char* originalFilename);

// Slanted "Outrun" PHOQER wordmark with flat chrome bands and speed lines, cropped to its pixels.
juce::Image renderOutrunWordmark(int character, bool onDarkBackground);
}
