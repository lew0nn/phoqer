#include "Logo.h"

#include <PhoqerUIAssets.h>

#include "Style.h"

#include <cmath>
#include <vector>

namespace phoqer::ui
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

// 8 x 11 letterforms; Q's tail drops to a 12th row.
const char* const glyphP[] { "######..", "#######.", "##...###", "##....##", "##...###", "#######.", "######..", "##......", "##......", "##......", "##......" };
const char* const glyphH[] { "##....##", "##....##", "##....##", "##....##", "##....##", "########", "########", "##....##", "##....##", "##....##", "##....##" };
const char* const glyphO[] { ".######.", "########", "##....##", "##....##", "##....##", "##....##", "##....##", "##....##", "##....##", "########", ".######." };
const char* const glyphQ[] { ".######.", "########", "##....##", "##....##", "##....##", "##....##", "##....##", "##..#.##", "##..####", "########", ".#######", "......##" };
const char* const glyphE[] { "########", "########", "##......", "##......", "##......", "######..", "######..", "##......", "##......", "########", "########" };
const char* const glyphR[] { "######..", "#######.", "##...###", "##....##", "##...###", "#######.", "######..", "##..##..", "##...##.", "##....##", "##....##" };
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

juce::Image loadAssetImage(const char* originalFilename)
{
    for (int i = 0; i < PhoqerUIAssets::namedResourceListSize; ++i)
    {
        const auto* name = PhoqerUIAssets::namedResourceList[i];
        if (juce::String(PhoqerUIAssets::getNamedResourceOriginalFilename(name)) == originalFilename)
        {
            int size = 0;
            const auto* data = PhoqerUIAssets::getNamedResource(name, size);
            return juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(size)).convertedToFormat(juce::Image::ARGB);
        }
    }
    jassertfalse;
    return {};
}
}
