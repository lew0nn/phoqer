#pragma once

#include <JuceHeader.h>

#include <functional>

namespace phoqer::ui
{
// Win98-style pixel dial: grey face with a 2 px 3D rim, a black pointer, and a value arc around it
// that fills in the voice colour. Drawn at one pixel per UI unit with hard edges.
class PixelKnob final : public juce::Slider
{
public:
    enum class Layout
    {
        mixer,    // name above (big) or below (small) the dial, value field underneath
        output    // dial on the left, name + value field on the right
    };

    struct Spec
    {
        juce::String name, description;
        float radius = 34.0f;
        float centreY = 82.0f;          // dial centre, from the top of the component
        bool big = true;                // big dials carry their name above, in the large font
        Layout layout = Layout::mixer;
        std::function<juce::String(double)> format;
    };

    explicit PixelKnob(Spec);

    void setCharacter(int character);
    void paint(juce::Graphics&) override;

private:
    Spec spec;
    int character = 0;
};

// The dial bitmap: a square centred on the dial, value arc included. 'proportion' is 0..1.
juce::Image renderDial98(int character, float radius, float proportion);
}
