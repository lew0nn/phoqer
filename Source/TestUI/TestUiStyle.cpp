#include "TestUiStyle.h"

#include <PhoqerTestUIAssets.h>

namespace phoqer::testui
{
namespace
{
juce::Typeface::Ptr loadTypeface(const char* originalFilename)
{
    for (int i = 0; i < PhoqerTestUIAssets::namedResourceListSize; ++i)
    {
        const auto* name = PhoqerTestUIAssets::namedResourceList[i];
        if (juce::String(PhoqerTestUIAssets::getNamedResourceOriginalFilename(name)) == originalFilename)
        {
            int size = 0;
            const auto* data = PhoqerTestUIAssets::getNamedResource(name, size);
            return juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size));
        }
    }
    jassertfalse;
    return {};
}
}

const Palette& paletteFor(int character) noexcept
{
    static const Palette burp { juce::Colour(0xffff2d55), juce::Colour(0xffff9f1c), juce::Colour(0xff12040a) };
    static const Palette squeal { juce::Colour(0xffe04bff), juce::Colour(0xff33e0ff), juce::Colour(0xff07031a) };
    static const Palette groan { juce::Colour(0xff6f9fbb), juce::Colour(0xffaeb0d4), juce::Colour(0xff060a14) };
    return character == 0 ? burp : character == 2 ? groan : squeal;
}

const Fonts& Fonts::get()
{
    static const Fonts fonts { loadTypeface("Silkscreen-Regular.ttf"), loadTypeface("Silkscreen-Bold.ttf"),
                               loadTypeface("PressStart2P-Regular.ttf"), loadTypeface("Roboto-Regular.ttf") };
    return fonts;
}

juce::Font pixelFont(float height, bool bold)
{
    return juce::Font(juce::FontOptions(bold ? Fonts::get().pixelBold : Fonts::get().pixel).withHeight(height));
}

juce::Font logoFont(float height)
{
    return juce::Font(juce::FontOptions(Fonts::get().logo).withHeight(height));
}

void bevel(juce::Graphics& g, juce::Rectangle<float> r, bool raised)
{
    using namespace win98;
    g.setColour(raised ? light : shadow);
    g.fillRect(r.getX(), r.getY(), r.getWidth(), 1.5f);
    g.fillRect(r.getX(), r.getY(), 1.5f, r.getHeight());
    g.setColour(raised ? dark : light);
    g.fillRect(r.getX(), r.getBottom() - 1.5f, r.getWidth(), 1.5f);
    g.fillRect(r.getRight() - 1.5f, r.getY(), 1.5f, r.getHeight());
    g.setColour(raised ? shadow : dark);
    if (raised)
    {
        g.fillRect(r.getX() + 1.5f, r.getBottom() - 3.0f, r.getWidth() - 3.0f, 1.5f);
        g.fillRect(r.getRight() - 3.0f, r.getY() + 1.5f, 1.5f, r.getHeight() - 3.0f);
    }
    else
    {
        g.fillRect(r.getX() + 1.5f, r.getY() + 1.5f, r.getWidth() - 3.0f, 1.5f);
        g.fillRect(r.getX() + 1.5f, r.getY() + 1.5f, 1.5f, r.getHeight() - 3.0f);
    }
}

void button98(juce::Graphics& g, juce::Rectangle<float> r, bool pressed)
{
    g.setColour(pressed ? juce::Colour(0xffb4b4b4) : win98::face);
    g.fillRect(r);
    bevel(g, r, ! pressed);
}

void sunken(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill)
{
    g.setColour(fill);
    g.fillRect(r);
    bevel(g, r, false);
}

juce::Rectangle<float> windowClient(juce::Rectangle<float> r) noexcept
{
    return { r.getX() + 6.0f, r.getY() + 24.0f, r.getWidth() - 12.0f, r.getHeight() - 30.0f };
}

juce::Rectangle<float> window98(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
                                juce::Colour titleColour, bool active)
{
    using namespace win98;
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(r.translated(4.0f, 4.0f));
    g.setColour(face);
    g.fillRect(r);
    bevel(g, r, true);
    const juce::Rectangle<float> bar { r.getX() + 4.0f, r.getY() + 4.0f, r.getWidth() - 8.0f, 16.0f };
    g.setColour(active ? titleColour : shadow);
    g.fillRect(bar);
    drawText(g, title, bar.reduced(4.0f, 0.0f), pixelFont(10.0f, true), active ? juce::Colours::white : juce::Colour(0xffd0d0d0));
    const int boxes = r.getWidth() < 120.0f ? 1 : 3;
    for (int k = 0; k < boxes; ++k)
    {
        const juce::Rectangle<float> b { bar.getRight() - 16.0f - k * 16.0f, bar.getY() + 2.0f, 14.0f, 12.0f };
        button98(g, b, false);
        g.setColour(juce::Colours::black);
        if (k == 0)
        {
            g.drawLine(b.getX() + 4, b.getY() + 3, b.getRight() - 4, b.getBottom() - 3, 1.5f);
            g.drawLine(b.getRight() - 4, b.getY() + 3, b.getX() + 4, b.getBottom() - 3, 1.5f);
        }
        if (k == 1) g.drawRect(b.reduced(3.5f, 3.0f), 1.0f);
        if (k == 2) g.fillRect(b.getX() + 4.0f, b.getBottom() - 4.0f, 6.0f, 1.5f);
    }
    return windowClient(r);
}

void statusBar(juce::Graphics& g, juce::Rectangle<float> r, const juce::StringArray& fields)
{
    float x = r.getX();
    const float each = r.getWidth() / static_cast<float>(juce::jmax(1, fields.size()));
    for (const auto& field : fields)
    {
        const juce::Rectangle<float> cell { x, r.getY(), each - 3.0f, r.getHeight() };
        bevel(g, cell, false);
        drawText(g, field, cell.reduced(5.0f, 0.0f), pixelFont(8.5f), juce::Colours::black);
        x += each;
    }
}

void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Font& font,
              juce::Colour colour, juce::Justification justification)
{
    g.setColour(colour);
    g.setFont(font);
    g.drawText(text, r, justification, false);
}

juce::Path modeGlyph(int mode, juce::Rectangle<float> r)
{
    juce::Path p;
    const auto c = r.getCentre();
    const float s = r.getWidth() * 0.5f;
    switch (mode)
    {
        case 0:
            for (int k = 0; k < 3; ++k)
                p.addCentredArc(c.x - s * 0.5f, c.y, s * (0.45f + 0.35f * k), s * (0.45f + 0.35f * k), 0.0f,
                                juce::MathConstants<float>::pi * 0.30f, juce::MathConstants<float>::pi * 0.70f, true);
            break;
        case 1:
            p.addEllipse(r.withSizeKeepingCentre(s * 1.6f, s * 1.6f));
            p.addEllipse(r.withSizeKeepingCentre(s * 0.8f, s * 0.8f));
            break;
        case 2:
            p.startNewSubPath(c.x - s * 0.9f, c.y + s * 0.5f);
            p.lineTo(c.x - s * 0.35f, c.y + s * 0.5f);
            p.lineTo(c.x - s * 0.1f, c.y - s * 0.8f);
            p.lineTo(c.x + s * 0.2f, c.y + s * 0.8f);
            p.lineTo(c.x + s * 0.45f, c.y - s * 0.2f);
            p.lineTo(c.x + s * 0.9f, c.y - s * 0.2f);
            break;
        case 3:
            p.startNewSubPath(c.x - s * 0.9f, c.y + s * 0.6f);
            p.cubicTo(c.x - s * 0.2f, c.y + s * 0.6f, c.x - s * 0.1f, c.y - s * 0.8f, c.x + s * 0.9f, c.y - s * 0.7f);
            break;
        default:
            p.startNewSubPath(c.x - s * 0.9f, c.y);
            for (int k = 1; k <= 8; ++k)
                p.lineTo(c.x - s * 0.9f + k * s * 0.225f, c.y + ((k % 2) != 0 ? -s * 0.22f : s * 0.22f));
            break;
    }
    return p;
}

float hash(int x, int y) noexcept
{
    uint32_t n = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return static_cast<float>(n & 0xffff) / 65535.0f;
}
}
