#include "Style.h"

#include <PhoqerUIAssets.h>

namespace phoqer::ui
{
namespace
{
juce::Typeface::Ptr loadTypeface(const char* originalFilename)
{
    for (int i = 0; i < PhoqerUIAssets::namedResourceListSize; ++i)
    {
        const auto* name = PhoqerUIAssets::namedResourceList[i];
        if (juce::String(PhoqerUIAssets::getNamedResourceOriginalFilename(name)) == originalFilename)
        {
            int size = 0;
            const auto* data = PhoqerUIAssets::getNamedResource(name, size);
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

juce::Rectangle<float> windowClient(juce::Rectangle<float> r, float titleHeight) noexcept
{
    return { r.getX() + 6.0f, r.getY() + 8.0f + titleHeight, r.getWidth() - 12.0f, r.getHeight() - 14.0f - titleHeight };
}

juce::Rectangle<float> windowTitleBar(juce::Rectangle<float> r, float titleHeight) noexcept
{
    return { r.getX() + 4.0f, r.getY() + 4.0f, r.getWidth() - 8.0f, titleHeight };
}

juce::Rectangle<float> window98(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
                                const Palette& palette, bool active, float titleHeight)
{
    using namespace win98;
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(r.translated(4.0f, 4.0f));
    g.setColour(face);
    g.fillRect(r);
    bevel(g, r, true);
    const auto bar = windowTitleBar(r, titleHeight);
    const auto from = active ? palette.accent.darker(0.35f) : palette.accent.darker(1.3f);
    const auto to = active ? palette.secondary.darker(0.15f) : palette.secondary.darker(1.6f);
    g.setGradientFill(juce::ColourGradient(from, bar.getX(), 0.0f, to, bar.getRight(), 0.0f, false));
    g.fillRect(bar);
    drawText(g, title, bar.reduced(4.0f, 0.0f), pixelFont(titleHeight >= 20.0f ? 12.0f : 10.0f, true), juce::Colours::white);
    return windowClient(r, titleHeight);
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

// ------------------------------------------------------------------------------- WIN98 LOOK AND FEEL
Win98LookAndFeel::Win98LookAndFeel()
{
    setColour(juce::PopupMenu::backgroundColourId, win98::face);
    setColour(juce::PopupMenu::textColourId, juce::Colours::black);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xffffffe1));
    setColour(juce::TooltipWindow::textColourId, juce::Colours::black);
    setColour(juce::TooltipWindow::outlineColourId, juce::Colours::black);
}

juce::Font Win98LookAndFeel::getPopupMenuFont() { return pixelFont(12.0f); }

void Win98LookAndFeel::drawPopupMenuBackgroundWithOptions(juce::Graphics& g, int width, int height, const juce::PopupMenu::Options&)
{
    const juce::Rectangle<float> r { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
    g.fillAll(win98::face);
    // Win98 raised frame: light grey / white on the top-left, black / grey on the bottom-right.
    g.setColour(juce::Colour(0xffdfdfdf)); g.fillRect(r.withHeight(1.0f)); g.fillRect(r.withWidth(1.0f));
    g.setColour(win98::light); g.fillRect(r.reduced(1.0f).withHeight(1.0f)); g.fillRect(r.reduced(1.0f).withWidth(1.0f));
    g.setColour(juce::Colours::black); g.fillRect(r.withTop(r.getBottom() - 1.0f)); g.fillRect(r.withLeft(r.getRight() - 1.0f));
    g.setColour(win98::shadow);
    g.fillRect(r.reduced(1.0f).withTop(r.getBottom() - 2.0f)); g.fillRect(r.reduced(1.0f).withLeft(r.getRight() - 2.0f));
}

void Win98LookAndFeel::drawPopupMenuItemWithOptions(juce::Graphics& g, const juce::Rectangle<int>& area, bool isHighlighted,
                                                    const juce::PopupMenu::Item& item, const juce::PopupMenu::Options&)
{
    auto r = area.toFloat();
    if (item.isSeparator)
    {
        const float y = std::round(r.getCentreY());
        g.setColour(win98::shadow); g.fillRect(r.getX() + 2.0f, y - 1.0f, r.getWidth() - 4.0f, 1.0f);
        g.setColour(win98::light); g.fillRect(r.getX() + 2.0f, y, r.getWidth() - 4.0f, 1.0f);
        return;
    }
    const bool hot = isHighlighted && item.isEnabled;
    if (hot)
    {
        g.setColour(paletteFor(character).accent.darker(0.35f));     // the title-bar colour
        g.fillRect(r.reduced(1.0f, 0.0f));
    }
    const auto ink = hot ? juce::Colours::white : juce::Colours::black;
    const auto textArea = r.withTrimmedLeft(22.0f).withTrimmedRight(10.0f);
    const auto font = getPopupMenuFont();
    if (! item.isEnabled)        // Win98 disabled text: grey with a white emboss
        drawText(g, item.text, textArea.translated(1.0f, 1.0f), font, win98::light);
    drawText(g, item.text, textArea, font, item.isEnabled ? ink : win98::shadow);
    if (item.shortcutKeyDescription.isNotEmpty())
        drawText(g, item.shortcutKeyDescription, textArea, pixelFont(8.0f), hot ? juce::Colour(0xffdfdfdf) : win98::shadow,
                 juce::Justification::centredRight);

    if (item.isTicked)
    {
        const float cx = r.getX() + 11.0f, cy = std::round(r.getCentreY());
        g.setColour(item.isEnabled ? ink : win98::shadow);
        if (item.itemID >= radioIdFirst && item.itemID <= radioIdLast)
        {
            g.fillRect(cx - 1.0f, cy - 3.0f, 3.0f, 6.0f);           // pixel radio dot
            g.fillRect(cx - 2.0f, cy - 2.0f, 5.0f, 4.0f);
        }
        else
        {
            const float pts[][2] { { -3, 0 }, { -2, 1 }, { -1, 2 }, { 0, 1 }, { 1, 0 }, { 2, -1 }, { 3, -2 } };
            for (const auto& p : pts) g.fillRect(cx + p[0], cy + p[1] - 1.0f, 1.0f, 3.0f);     // pixel checkmark
        }
    }
}

void Win98LookAndFeel::getIdealPopupMenuItemSizeWithOptions(const juce::String& text, bool isSeparator, int, int& idealWidth,
                                                            int& idealHeight, const juce::PopupMenu::Options&)
{
    idealHeight = isSeparator ? 8 : 20;
    idealWidth = juce::roundToInt(juce::GlyphArrangement::getStringWidth(getPopupMenuFont(), text)) + 70;
}

juce::Rectangle<int> Win98LookAndFeel::getTooltipBounds(const juce::String& text, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(pixelFont(12.0f), text)) + 14, h = 20;
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin(parentArea);
}

void Win98LookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const juce::Rectangle<float> r { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
    g.fillAll(juce::Colour(0xffffffe1));
    g.setColour(juce::Colours::black);
    g.drawRect(r, 1.0f);
    drawText(g, text, r.reduced(7.0f, 0.0f), pixelFont(12.0f), juce::Colours::black);
}

void Win98LookAndFeel::drawCornerResizer(juce::Graphics& g, int width, int height, bool, bool)
{
    // Win98 size grip: three diagonal ridges, each a white line over a grey one.
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    for (int ridge = 0; ridge < 3; ++ridge)
    {
        const float off = 3.0f + static_cast<float>(ridge) * 4.0f;
        for (float t = 0.0f; t < w - off; t += 1.0f)
        {
            g.setColour(win98::light);  g.fillRect(off + t, h - 1.0f - t, 1.0f, 1.0f);
            g.setColour(win98::shadow); g.fillRect(off + t + 1.0f, h - 1.0f - t, 1.0f, 1.0f);
            g.fillRect(off + t + 2.0f, h - 1.0f - t, 1.0f, 1.0f);
        }
    }
}
}
