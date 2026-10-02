#include "TestUiKnob.h"

#include "TestUiStyle.h"

#include <cmath>

namespace phoqer::testui
{
namespace
{
constexpr float startAngle = -2.35619449f, sweep = 4.71238898f;   // slider angles: radians, 0 = up, clockwise
constexpr float arcStart = 2.35619449f;                           // the same start in screen angles (0 = right, y down)

const juce::Colour lite2 { 0xffdfdfdf };
}

juce::Image renderDial98(int character, float r, float proportion)
{
    const auto& pal = paletteFor(character);
    // Chunky enough to read at 100% display scale: big dials get a 4 px arc and rim, small ones 3 px.
    const bool big = r >= 30.0f;
    const float arcGap = 2.0f, arcWidth = big ? 4.0f : 3.0f, rimOuter = 2.0f, rimInner = big ? 2.0f : 1.0f;
    const float pointerHalf = big ? 2.6f : 1.9f;
    const int n = static_cast<int>(std::ceil(r + arcGap + arcWidth)) * 2 + 4;
    const float c = static_cast<float>(n) * 0.5f;
    const float ux = std::cos(arcStart + sweep * proportion), uy = std::sin(arcStart + sweep * proportion);
    juce::Image image(juce::Image::ARGB, n, n, true);
    juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const float dx = static_cast<float>(x) + 0.5f - c, dy = static_cast<float>(y) + 0.5f - c;
            const float d = std::hypot(dx, dy);
            const float lit = (-dx - dy) / (juce::MathConstants<float>::sqrt2 * juce::jmax(d, 1.0e-6f));
            juce::Colour px;

            // Value track outside the rim: grey, filled with the voice colour up to the value.
            if (d >= r + arcGap && d < r + arcGap + arcWidth)
            {
                const float along = std::fmod(std::atan2(dy, dx) - arcStart + juce::MathConstants<float>::twoPi * 2.0f,
                                              juce::MathConstants<float>::twoPi);
                if (along <= sweep)
                    px = (proportion > 0.0f && along <= sweep * proportion) ? pal.accent : win98::shadow;
            }
            // Face and the Win98 rim (light top-left, dark bottom-right): an outer and an inner band.
            else if (d < r - rimOuter - rimInner)
                px = win98::face;
            else if (d < r - rimOuter)
                px = lit > 0.3f ? lite2 : lit < -0.3f ? win98::shadow : win98::face;
            else if (d < r)
                px = lit > 0.3f ? win98::light : lit < -0.3f ? juce::Colours::black : win98::shadow;

            // Pointer: a solid black bar from near the centre to inside the rim.
            if (d < r - rimOuter - rimInner)
            {
                const float along = dx * ux + dy * uy, across = std::abs(-dx * uy + dy * ux);
                if (along > 3.0f && along < r - rimOuter - rimInner - 3.0f && across < pointerHalf) px = juce::Colours::black;
            }
            if (! px.isTransparent()) data.setPixelColour(x, y, px);
        }
    return image;
}

PixelKnob::PixelKnob(Spec s) : spec(std::move(s))
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters(juce::MathConstants<float>::twoPi + startAngle, juce::MathConstants<float>::twoPi + startAngle + sweep, true);
    setMouseDragSensitivity(220);
    setVelocityBasedMode(false);
    setName(spec.name);
    setTooltip(spec.name + (spec.description.isNotEmpty() ? " - " + spec.description : juce::String()));
}

void PixelKnob::setCharacter(int c)
{
    if (c == character) return;
    character = c;
    repaint();
}

void PixelKnob::paint(juce::Graphics& g)
{
    const auto& pal = paletteFor(character);
    const float r = spec.radius;
    const juce::Point<float> c = spec.layout == Layout::output ? juce::Point<float>(r + 10.0f, std::round(getHeight() * 0.5f))
                                                              : juce::Point<float>(std::round(getWidth() * 0.5f), spec.centreY);

    // The dial, one bitmap pixel per UI unit, scaled without smoothing.
    const auto dial = renderDial98(character, r, static_cast<float>(valueToProportionOfLength(getValue())));
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.setOpacity(1.0f);
    g.drawImage(dial, juce::Rectangle<float>(static_cast<float>(dial.getWidth()), static_cast<float>(dial.getHeight())).withCentre(c));

    const auto valueText = spec.format ? spec.format(getValue()) : juce::String(getValue(), 2);
    const auto valueInk = pal.accent.darker(0.6f);
    if (spec.layout == Layout::mixer)
    {
        const float w = static_cast<float>(getWidth());
        if (spec.big)
            drawText(g, spec.name, { 0.0f, 10.0f, w, 16.0f }, pixelFont(16.0f, true), juce::Colours::black, juce::Justification::centred);
        else
            drawText(g, spec.name, { 0.0f, 111.0f, w, 10.0f }, pixelFont(8.0f, true), juce::Colours::black, juce::Justification::centred);
        const float boxW = spec.big ? 68.0f : 58.0f;
        const juce::Rectangle<float> box { std::round((w - boxW) * 0.5f), 134.0f, boxW, 26.0f };
        sunken(g, box);
        drawText(g, valueText, box, pixelFont(16.0f, true), valueInk, juce::Justification::centred);
    }
    else
    {
        const float x = c.x + r + 12.0f;
        drawText(g, spec.name, { x, 4.0f, getWidth() - x, 14.0f }, pixelFont(9.0f, true), juce::Colours::black);
        const juce::Rectangle<float> box { x, 20.0f, juce::jmin(84.0f, getWidth() - x - 2.0f), 22.0f };
        sunken(g, box);
        drawText(g, valueText, box, pixelFont(16.0f, true), valueInk, juce::Justification::centred);
    }
}
}
