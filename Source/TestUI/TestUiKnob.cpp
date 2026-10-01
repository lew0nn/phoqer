#include "TestUiKnob.h"

#include "TestUiStyle.h"

#include <cmath>

namespace phoqer::testui
{
namespace
{
constexpr float startAngle = -2.35f, sweep = 4.7f;   // radians, 0 = up, clockwise

// Brightness (0..1) of the machined silver knob at (x, y) in [-1, 1]; alpha via 'coverage'.
float knobBrightness(float x, float y, float& coverage)
{
    const float r = std::hypot(x, y);
    coverage = juce::jlimit(0.0f, 1.0f, (1.0f - r) * 60.0f);
    if (coverage <= 0.0f) return 0.0f;
    const float a = std::atan2(x, -y);
    const float light = -(x * 0.55f + y * 0.83f) / juce::jmax(0.001f, r);
    constexpr float faceR = 0.78f;
    if (r > faceR + 0.035f)
    {
        // Knurled rim.
        const bool ridge = std::fmod(a / juce::MathConstants<float>::twoPi * 120.0f + 100.0f, 1.0f) < 0.5f;
        float v = 0.55f + 0.35f * (0.5f + 0.5f * light) + (ridge ? 0.06f : -0.06f);
        if (r > 0.985f) v *= 0.6f;
        return v;
    }
    if (r > faceR)
        return 0.45f + 0.55f * (0.5f + 0.5f * light);       // polished chamfer
    // Concentric machining: bow-tie highlight along the light axis, faint ring noise.
    const float lobe = std::pow(std::abs(std::cos(a + 0.6f)), 6.0f);
    float v = 0.66f + 0.30f * lobe - 0.10f * std::pow(std::abs(std::sin(a + 0.6f)), 4.0f)
            + 0.05f * (hash(static_cast<int>(r * 900.0f), 17) - 0.5f);
    if (r < 0.06f) v *= 0.85f;
    return v;
}

juce::Point<float> onCircle(juce::Point<float> c, float radius, float angle)
{
    return { c.x + radius * std::sin(angle), c.y - radius * std::cos(angle) };
}

juce::Point<float> snap(juce::Point<float> p)
{
    return { std::round(p.x / 2.0f) * 2.0f, std::round(p.y / 2.0f) * 2.0f };
}
}

// TEMPORARY treatment ("C3"): darkened palette tint, five bands, checker only at band edges.
juce::Image renderDigitizedKnob(int n, int character)
{
    const auto& pal = paletteFor(character);
    const juce::Colour tint[] { pal.sky, pal.accent.darker(1.6f), pal.accent.darker(1.1f), pal.accent.darker(0.65f),
                                pal.accent.interpolatedWith(pal.secondary, 0.4f).darker(0.35f), pal.secondary.darker(0.1f) };
    constexpr int levels = 5;
    juce::Image out(juce::Image::ARGB, n, n, true);
    juce::Image::BitmapData data(out, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            // 4x4 supersampled brightness of this bitmap pixel.
            float sum = 0.0f, cover = 0.0f;
            for (int j = 0; j < 4; ++j)
                for (int i = 0; i < 4; ++i)
                {
                    const float u = (static_cast<float>(x) + (static_cast<float>(i) + 0.5f) / 4.0f) / static_cast<float>(n) * 2.0f - 1.0f;
                    const float v = (static_cast<float>(y) + (static_cast<float>(j) + 0.5f) / 4.0f) / static_cast<float>(n) * 2.0f - 1.0f;
                    float c = 0.0f;
                    sum += knobBrightness(u, v, c) * c;
                    cover += c;
                }
            if (cover / 16.0f < 0.45f) continue;
            const float lum = std::pow(juce::jlimit(0.0f, 1.0f, sum / cover), 1.6f) * 0.92f;
            const float f = lum * (levels - 1);
            int level = static_cast<int>(std::floor(f));
            const float t = f - static_cast<float>(level);
            if (t > 0.62f || (t > 0.38f && ((x + y) & 1) != 0)) ++level;
            data.setPixelColour(x, y, tint[juce::roundToInt(juce::jlimit(0, levels - 1, level) * 5.0f / (levels - 1))]);
        }
    return out;
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

const juce::Image& PixelKnob::knobBitmap()
{
    if (cachedCharacter != character || ! cached.isValid())
    {
        cached = renderDigitizedKnob(juce::roundToInt(spec.diameter / 2.0f), character);
        cachedCharacter = character;
    }
    return cached;
}

void PixelKnob::paint(juce::Graphics& g)
{
    const auto& pal = paletteFor(character);
    const auto ink = juce::Colours::black;
    const auto outline = juce::Colour(0xff1a1020);
    const float norm = static_cast<float>(valueToProportionOfLength(getValue()));
    const float d = spec.diameter, r = d * 0.5f;
    const juce::Point<float> c = spec.layout == Layout::output ? juce::Point<float>(2.0f + r + 6.0f, getHeight() * 0.5f)
                                                              : juce::Point<float>(std::round(getWidth() * 0.5f), spec.centreY);

    // Printed scale (mixer layout).
    if (spec.layout == Layout::mixer)
    {
        g.setColour(ink);
        const float scaleOffset = spec.showNumbers ? 9.0f : 6.0f;   // keeps neighbouring small scales apart
        for (int t = 0; t <= 20; ++t)
        {
            const bool major = t % 2 == 0;
            if (! major && ! spec.showNumbers) continue;
            const float ang = startAngle + sweep * static_cast<float>(t) / 20.0f;
            for (int k = 0; k < (major ? 3 : 2); ++k)
                g.fillRect(juce::Rectangle<float>(2.0f, 2.0f).withCentre(snap(onCircle(c, r + scaleOffset + k * 2.0f, ang))));
            if (spec.showNumbers && t % 4 == 0)
                drawText(g, juce::String(t / 2), juce::Rectangle<float>(16.0f, 10.0f).withCentre(snap(onCircle(c, r + 21.0f, ang))),
                         pixelFont(8.0f), ink, juce::Justification::centred);
        }
    }

    // Digitized knob bitmap, nearest-neighbour at 2 units per bitmap pixel.
    const auto& bitmap = knobBitmap();
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.setOpacity(1.0f);
    g.drawImage(bitmap, juce::Rectangle<float>(static_cast<float>(bitmap.getWidth()) * 2.0f, static_cast<float>(bitmap.getHeight()) * 2.0f)
                            .withCentre(snap(c)));

    // Crisp sprite outline.
    g.setColour(outline);
    const int steps = static_cast<int>(r * 4.0f);
    for (int k = 0; k < steps; ++k)
        g.fillRect(juce::Rectangle<float>(2.0f, 2.0f).withCentre(snap(onCircle(c, r + 0.5f, juce::MathConstants<float>::twoPi * k / steps))));

    // Value LED set into the knob face.
    const auto led = snap(onCircle(c, r * 0.62f, startAngle + sweep * norm));
    const float sz = d > 50.0f ? 4.0f : 2.0f;
    g.setColour(pal.accent.withAlpha(0.35f));
    g.fillRect(juce::Rectangle<float>(sz + 8.0f, sz + 8.0f).withCentre(led));
    g.setColour(outline);
    g.fillRect(juce::Rectangle<float>(sz + 4.0f, sz + 4.0f).withCentre(led));
    g.setColour(pal.accent.interpolatedWith(juce::Colours::white, 0.4f));
    g.fillRect(juce::Rectangle<float>(sz + 2.0f, sz + 2.0f).withCentre(led));
    g.setColour(juce::Colours::white);
    g.fillRect(juce::Rectangle<float>(sz, sz).withCentre(led));

    const auto valueText = spec.format ? spec.format(getValue()) : juce::String(getValue(), 2);
    if (spec.layout == Layout::mixer)
    {
        const bool big = spec.showNumbers;
        drawText(g, spec.name, { 0.0f, getHeight() - 40.0f, static_cast<float>(getWidth()), 14.0f }, pixelFont(big ? 12.0f : 10.0f, true),
                 ink, juce::Justification::centred);
        drawText(g, big ? spec.description + "  " + valueText : valueText, { 0.0f, getHeight() - 24.0f, static_cast<float>(getWidth()), 12.0f },
                 pixelFont(8.0f), ink.withAlpha(0.65f), juce::Justification::centred);
    }
    else
    {
        const float x = c.x + r + 12.0f;
        drawText(g, spec.name, { x, 4.0f, getWidth() - x, 14.0f }, pixelFont(9.0f, true), ink);
        const juce::Rectangle<float> box { x, 22.0f, juce::jmin(52.0f, getWidth() - x - 2.0f), 16.0f };
        sunken(g, box);
        drawText(g, valueText, box, pixelFont(8.5f), ink, juce::Justification::centred);
    }
}
}
