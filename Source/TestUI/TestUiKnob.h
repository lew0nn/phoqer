#pragma once

#include <JuceHeader.h>

#include <functional>

namespace phoqer::testui
{
// Digitized Yamaha-style hi-fi knob: a machined silver knob rendered as a palette bitmap in the
// character's colours, with a crisp pixel outline, value LED, printed scale and labels.
class PixelKnob final : public juce::Slider
{
public:
    enum class Layout
    {
        mixer,    // scale around the knob, name + description/value underneath
        output    // bare knob on the left, name + value box on the right
    };

    struct Spec
    {
        juce::String name, description;
        float diameter = 70.0f;
        float centreY = 84.0f;          // knob centre, from the top of the component
        bool showNumbers = true;        // 0..10 numerals on the scale
        Layout layout = Layout::mixer;
        std::function<juce::String(double)> format;
    };

    explicit PixelKnob(Spec);

    void setCharacter(int character);
    void paint(juce::Graphics&) override;

private:
    const juce::Image& knobBitmap();

    Spec spec;
    int character = 1;
    juce::Image cached;
    int cachedCharacter = -1;
};

// TEMPORARY: the knob's digitized look ("C3", pixel-art edge dither) is a placeholder chosen
// for the test UI. Everything about it lives in renderDigitizedKnob(); replace that function
// to change the treatment.
juce::Image renderDigitizedKnob(int bitmapSize, int character);
}
