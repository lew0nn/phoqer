#include "TestUiLogo.h"

#include "TestUiStyle.h"

#include <cmath>
#include <vector>

namespace phoqer::testui
{
namespace
{
const juce::Colour darkInk { 0xff0a0a0a };

// A boolean pixel mask with the few morphology operations the logo needs.
struct Mask
{
    int w = 0, h = 0;
    std::vector<char> bits;

    Mask(int width, int height) : w(width), h(height), bits(static_cast<size_t>(width * height), 0) {}
    bool get(int x, int y) const noexcept { return x >= 0 && y >= 0 && x < w && y < h && bits[static_cast<size_t>(y * w + x)] != 0; }
    void set(int x, int y, bool v = true) noexcept { if (x >= 0 && y >= 0 && x < w && y < h) bits[static_cast<size_t>(y * w + x)] = v ? 1 : 0; }

    Mask shifted(int dx, int dy) const
    {
        Mask out(w, h);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                out.set(x, y, get(x - dx, y - dy));
        return out;
    }
    Mask dilated() const
    {
        Mask out(w, h);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                for (int dy = -1; dy <= 1 && ! out.get(x, y); ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (get(x + dx, y + dy)) { out.set(x, y); break; }
        return out;
    }
    Mask outline() const
    {
        auto out = dilated();
        for (size_t i = 0; i < bits.size(); ++i) if (bits[i]) out.bits[i] = 0;
        return out;
    }
};

void fill(juce::Image& image, const Mask& m, juce::Colour c)
{
    for (int y = 0; y < m.h; ++y)
        for (int x = 0; x < m.w; ++x)
            if (m.get(x, y)) image.setPixelAt(x, y, c);
}

bool inEllipse(float x, float y, float cx, float cy, float rx, float ry) noexcept
{
    const float u = (x - cx) / rx, v = (y - cy) / ry;
    return u * u + v * v <= 1.0f;
}

// Rounded icon tile on the 32 x 32 design grid.
bool inTile(float x, float y, float r = 4.5f) noexcept
{
    if (x < 0.0f || y < 0.0f || x >= 32.0f || y >= 32.0f) return false;
    const bool cornerX = x < r || x > 32.0f - r, cornerY = y < r || y > 32.0f - r;
    if (! (cornerX && cornerY)) return true;
    const float cx = x < 16.0f ? r : 32.0f - r, cy = y < 16.0f ? r : 32.0f - r;
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
}

// Flat Outrun bands, top to bottom: pale secondary, secondary, pale accent, accent, dark accent.
juce::Colour band(const Palette& pal, float y, const float (&edges)[4]) noexcept
{
    const juce::Colour cols[] { pal.secondary.interpolatedWith(juce::Colours::white, 0.55f), pal.secondary,
                                pal.accent.interpolatedWith(juce::Colours::white, 0.35f), pal.accent, pal.accent.darker(0.55f) };
    int i = 0;
    while (i < 4 && y >= edges[i]) ++i;
    return cols[i];
}

// 8 x 11 letterforms; Q's tail drops to a 12th row.
const char* const glyphP[] { "######..", "#######.", "##...###", "##....##", "##...###", "#######.", "######..", "##......", "##......", "##......", "##......" };
const char* const glyphH[] { "##....##", "##....##", "##....##", "##....##", "##....##", "########", "########", "##....##", "##....##", "##....##", "##....##" };
const char* const glyphO[] { ".######.", "########", "##....##", "##....##", "##....##", "##....##", "##....##", "##....##", "##....##", "########", ".######." };
const char* const glyphQ[] { ".######.", "########", "##....##", "##....##", "##....##", "##....##", "##....##", "##..#.##", "##..####", "########", ".#######", "......##" };
const char* const glyphE[] { "########", "########", "##......", "##......", "##......", "######..", "######..", "##......", "##......", "########", "########" };
const char* const glyphR[] { "######..", "#######.", "##...###", "##....##", "##...###", "#######.", "######..", "##..##..", "##...##.", "##....##", "##....##" };
}

juce::Image renderAppIcon(int character, int n, bool framed)
{
    const auto& pal = paletteFor(character);
    const float px = 32.0f / static_cast<float>(n);
    const bool big = n >= 32;
    constexpr float horizon = 23.0f;      // the waterline, in the 32 x 32 design space
    auto centre = [px](int i) { return (static_cast<float>(i) + 0.5f) * px; };
    auto inBox = [](float x, float y, float x0, float y0, float x1, float y1) { return x >= x0 && x < x1 && y >= y0 && y < y1; };

    Mask tile(n, n), sun(n, n), eyes(n, n), shine(n, n), muzzle(n, n), nose(n, n), sea(n, n), bars(n, n), waterline(n, n);
    for (int iy = 0; iy < n; ++iy)
        for (int ix = 0; ix < n; ++ix)
        {
            const float x = centre(ix), y = centre(iy);
            const bool t = inTile(x, y);
            tile.set(ix, iy, t);
            const bool s = inEllipse(x, y, 16.0f, 16.5f, 12.5f, 12.5f) && y < horizon && ! (y >= 21.5f && y < 21.5f + juce::jmax(0.9f, px));
            sun.set(ix, iy, s);
            eyes.set(ix, iy, inEllipse(x, y, 11.2f, 12.8f, 1.7f, 2.1f) || inEllipse(x, y, 20.8f, 12.8f, 1.7f, 2.1f));
            shine.set(ix, iy, big && (inBox(x, y, 10.2f, 11.4f, 10.2f + px, 11.4f + px) || inBox(x, y, 19.8f, 11.4f, 19.8f + px, 11.4f + px)));
            muzzle.set(ix, iy, s && (inEllipse(x, y, 13.4f, 18.6f, 3.6f, 2.8f) || inEllipse(x, y, 18.6f, 18.6f, 3.6f, 2.8f)));
            nose.set(ix, iy, inEllipse(x, y, 16.0f, 16.4f, 2.0f, 1.3f));
            sea.set(ix, iy, t && y >= horizon);
            waterline.set(ix, iy, t && y >= horizon && y < horizon + px);
            const float barRows[][2] { { horizon + 1.6f, 9.0f }, { horizon + 3.8f, 6.0f }, { horizon + 6.0f, 3.0f } };
            bool b = false;
            for (const auto& r : barRows) b = b || inBox(x, y, 16.0f - r[1], r[0], 16.0f + r[1], r[0] + juce::jmax(px, 1.0f));
            bars.set(ix, iy, t && y >= horizon && b);
        }

    // Voice-coloured frame just inside the tile edge: 1 px, 2 px from 32 px up.
    auto erode = [](const Mask& m)
    {
        Mask out(m.w, m.h);
        const auto l = m.shifted(1, 0), r = m.shifted(-1, 0), u = m.shifted(0, 1), d = m.shifted(0, -1);
        for (size_t i = 0; i < out.bits.size(); ++i) out.bits[i] = (m.bits[i] && l.bits[i] && r.bits[i] && u.bits[i] && d.bits[i]) ? 1 : 0;
        return out;
    };
    const auto inner = erode(tile);
    Mask frame(n, n);
    const auto inner2 = erode(inner);
    for (size_t i = 0; i < frame.bits.size(); ++i)
        frame.bits[i] = framed && ((tile.bits[i] && ! inner.bits[i]) || (big && inner.bits[i] && ! inner2.bits[i])) ? 1 : 0;

    juce::Image image(juce::Image::ARGB, n, n, true);
    fill(image, tile, pal.sky);
    const float edges[4] { 8.0f, 12.0f, 14.5f, 21.5f };
    for (int iy = 0; iy < n; ++iy)
        for (int ix = 0; ix < n; ++ix)
            if (sun.get(ix, iy) && tile.get(ix, iy)) image.setPixelAt(ix, iy, band(pal, centre(iy), edges));
    fill(image, muzzle, pal.secondary.interpolatedWith(juce::Colours::white, 0.6f));
    for (int iy = 0; iy < n; ++iy)
        for (int ix = 0; ix < n; ++ix)
        {
            if (sun.get(ix, iy) && eyes.get(ix, iy)) image.setPixelAt(ix, iy, darkInk);
            if (shine.get(ix, iy) && eyes.get(ix, iy)) image.setPixelAt(ix, iy, juce::Colours::white);
            if (sun.get(ix, iy) && nose.get(ix, iy)) image.setPixelAt(ix, iy, darkInk);
        }
    fill(image, sea, pal.sky.interpolatedWith(pal.secondary, 0.10f));
    fill(image, bars, pal.accent.interpolatedWith(pal.secondary, 0.35f));
    fill(image, waterline, pal.accent.interpolatedWith(pal.secondary, 0.5f));
    fill(image, frame, pal.accent);
    fill(image, tile.outline(), darkInk);

    // Whiskers: light, blended 65% over what is underneath so they read thin.
    Mask whiskers(n, n);
    constexpr float cy = 18.0f, innerX = 6.0f, outerX = 14.6f, spread = 1.5f;
    for (const int side : { -1, 1 })
    {
        if (big)
        {
            const float rows[][2] { { cy - spread, -0.2f }, { cy, 0.0f }, { cy + spread, 0.2f } };
            for (const auto& row : rows)
                for (int k = 0; k < 120; ++k)
                {
                    const float x = 16.0f + static_cast<float>(side) * (innerX + (outerX - innerX) * static_cast<float>(k) / 119.0f);
                    const float y = row[0] + row[1] * std::abs(x - 16.0f - static_cast<float>(side) * innerX);
                    whiskers.set(static_cast<int>(x / px), static_cast<int>(y / px));
                }
        }
        else
        {
            for (const int dy : { 0, 2 })
            {
                const int y = static_cast<int>(cy / px) + dy - 1;
                for (int x = static_cast<int>((16.0f + static_cast<float>(side) * innerX) / px);
                     x != static_cast<int>((16.0f + static_cast<float>(side) * outerX) / px); x += side)
                    whiskers.set(x, y);
            }
        }
    }
    const auto light = pal.secondary.interpolatedWith(juce::Colours::white, 0.55f);
    for (int iy = 0; iy < n; ++iy)
        for (int ix = 0; ix < n; ++ix)
            if (whiskers.get(ix, iy) && ! frame.get(ix, iy) && centre(iy) < horizon)
                image.setPixelAt(ix, iy, image.getPixelAt(ix, iy).withAlpha(1.0f).interpolatedWith(light, 0.65f));
    return image;
}

juce::Image renderOutrunWordmark(int character, bool onDarkBackground)
{
    const auto& pal = paletteFor(character);
    const char* const* word[] { glyphP, glyphH, glyphO, glyphQ, glyphE, glyphR };
    const int wordRows[] { 11, 11, 11, 12, 11, 11 };
    constexpr int letterH = 12, letterW = 8, gap = 2, count = 6, pad = 3;
    constexpr int plainW = count * letterW + (count - 1) * gap;
    constexpr int slantW = plainW + letterH / 2 + 2;
    Mask shape(slantW + pad * 2, letterH + pad * 2);
    for (int i = 0; i < count; ++i)
        for (int y = 0; y < wordRows[i]; ++y)
            for (int x = 0; x < letterW; ++x)
                if (word[i][y][x] == '#')
                    shape.set(pad + i * (letterW + gap) + x + (letterH - 1 - y) / 2, pad + y);

    Mask speed(shape.w, shape.h);
    const int lines[][2] { { 5, 10 }, { 8, 14 }, { 11, 7 } };
    for (const auto& l : lines)
        for (int x = 0; x < l[1]; ++x) speed.set(x, l[0]);
    const auto halo = shape.dilated();
    for (size_t i = 0; i < speed.bits.size(); ++i) if (halo.bits[i]) speed.bits[i] = 0;

    juce::Image image(juce::Image::ARGB, shape.w, shape.h, true);
    fill(image, halo.shifted(2, 1), pal.accent.darker(1.4f));
    fill(image, shape.outline(), darkInk);
    for (int y = 0; y < shape.h; ++y)
        for (int x = 0; x < shape.w; ++x)
            if (shape.get(x, y))
            {
                const int r = y - pad;
                auto c = r < 4 ? pal.secondary.interpolatedWith(juce::Colours::white, 0.55f) : r < 6 ? pal.secondary : pal.accent;
                if (r == 6) c = pal.accent.interpolatedWith(juce::Colours::white, 0.35f);
                image.setPixelAt(x, y, c);
            }
    fill(image, speed, onDarkBackground ? pal.secondary : pal.accent.darker(0.2f));

    // Crop to the drawn pixels.
    int x0 = image.getWidth(), y0 = image.getHeight(), x1 = -1, y1 = -1;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            if (image.getPixelAt(x, y).getAlpha() > 0) { x0 = juce::jmin(x0, x); y0 = juce::jmin(y0, y); x1 = juce::jmax(x1, x); y1 = juce::jmax(y1, y); }
    return x1 < 0 ? image : image.getClippedImage({ x0, y0, x1 - x0 + 1, y1 - y0 + 1 }).createCopy();
}
}
