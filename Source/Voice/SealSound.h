#pragma once

#include <JuceHeader.h>

namespace phoqer
{
class SealSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
}

