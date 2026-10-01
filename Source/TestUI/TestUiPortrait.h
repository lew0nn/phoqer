#pragma once

#include <JuceHeader.h>

#include "../Core/FaceTelemetry.h"
#include "TestUiStyle.h"

#include <array>
#include <vector>

namespace phoqer::testui
{
// What the portrait shows: the engine's face telemetry plus the manic grin amount (0..1).
struct Expression
{
    FaceTelemetry face;
    float grin = 0.0f;
};

// SEAL.BMP: the approved painted portraits, warped live into facial expressions and reduced to
// a dithered palette bitmap in the character's colours.
class PortraitRenderer
{
public:
    PortraitRenderer();

    // Renders a width x height palette bitmap (one bitmap pixel per image pixel).
    juce::Image render(int character, const Expression&, int width, int height) const;

private:
    struct Rgb { float r = 0, g = 0, b = 0; };
    struct Painting
    {
        int width = 0, height = 0;
        std::vector<Rgb> pixels;
        Rgb sample(float x, float y) const noexcept;
    };
    struct Eye { juce::Point<float> centre; float rx, ry; };
    struct Landmarks
    {
        std::array<Eye, 2> eyes;
        juce::Point<float> nose, mouth;
        float mouthHalf;
        std::array<juce::Point<float>, 2> pads;
        juce::Point<float> pivot;
    };

    static const Landmarks& landmarksFor(int character) noexcept;
    Rgb expressive(const Painting&, const Landmarks&, const Expression&, float x, float y) const noexcept;

    std::array<Painting, 3> paintings;
};
}
