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

namespace
{
void paintBootStars(juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::Random random(11);
    for (int k = 0; k < 24; ++k)
    {
        g.setColour(juce::Colours::white.withAlpha(0.2f + 0.5f * random.nextFloat()));
        g.fillRect(area.getX() + std::round(random.nextFloat() * area.getWidth() / 2.0f) * 2.0f,
                   area.getY() + std::round(random.nextFloat() * area.getHeight() / 2.0f) * 2.0f,
                   2.0f, 2.0f);
    }
    g.setOpacity(1.0f);
}

void paintBootProgress(juce::Graphics& g, juce::Rectangle<float> area, float progress, juce::Colour colour)
{
    sunken(g, area);
    const auto inner = area.reduced(3.0f);
    const int count = static_cast<int>(inner.getWidth() / 10.0f);
    const int lit = juce::roundToInt(progress * static_cast<float>(count));
    g.setColour(colour);
    for (int block = 0; block < lit; ++block)
        g.fillRect(inner.getX() + static_cast<float>(block) * 10.0f,
                   inner.getY(), 8.0f, inner.getHeight());
}
}

void paintStandaloneBootSplash(juce::Graphics& g, juce::Rectangle<float> bounds, float progress)
{
    const auto& palette = paletteFor(0);
    static const auto icon = loadAssetImage("phoqer-app-icon-256.png");
    static const auto wordmark = renderOutrunWordmark(0, true);
    const float wordmarkWidth = static_cast<float>(wordmark.getWidth()) * 2.0f;
    const float wordmarkHeight = static_cast<float>(wordmark.getHeight()) * 2.0f;
    const char* voices[] { "BURP", "SQUEAL", "GROAN" };

    progress = juce::jlimit(0.0f, 1.0f, progress);
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.fillAll(win98::face);

    // Win98 installer structure: active title bar, artwork bay, status column and block meter.
    const auto client = window98(g, bounds, "PHOQER.EXE", palette, true);
    const auto artwork = client.withWidth(150.0f).reduced(2.0f);
    g.setColour(palette.sky);
    g.fillRect(artwork);
    paintBootStars(g, artwork);
    if (icon.isValid())
        g.drawImage(icon, juce::Rectangle<float>(112.0f, 112.0f).withCentre(artwork.getCentre()));

    const auto status = client.withTrimmedLeft(162.0f).reduced(8.0f, 10.0f);
    g.drawImage(wordmark, { status.getX(), status.getY(), wordmarkWidth, wordmarkHeight });
    drawText(g, "VERSION " JucePlugin_VersionString,
             { status.getX(), status.getY() + wordmarkHeight + 4.0f, status.getWidth(), 14.0f },
             pixelFont(11.0f), win98::shadow);

    float y = status.getY() + wordmarkHeight + 30.0f;
    for (int voice = 0; voice < 3; ++voice)
    {
        const float stage = progress * 3.0f;
        const bool complete = stage > static_cast<float>(voice) + 0.6f;
        const bool current = ! complete && stage > static_cast<float>(voice) - 0.4f;
        const auto marker = complete ? "[X] " : current ? "[>] " : "[ ] ";
        drawText(g, juce::String(marker) + "WAKING " + voices[voice] + "...",
                 { status.getX(), y, status.getWidth(), 16.0f }, pixelFont(12.0f),
                 complete || current ? juce::Colours::black : win98::shadow);
        y += 22.0f;
    }

    paintBootProgress(g, { status.getX(), status.getBottom() - 18.0f, status.getWidth(), 18.0f },
                      progress, palette.accent.darker(0.35f));
}
}
