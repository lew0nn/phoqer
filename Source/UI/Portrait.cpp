#include "Portrait.h"

#include <PhoqerUIAssets.h>

#include <cmath>

namespace phoqer::ui
{
namespace
{
float smoothstep(float a, float b, float x) noexcept
{
    const float t = juce::jlimit(0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

juce::Image loadImage(const char* originalFilename)
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

PortraitRenderer::Rgb PortraitRenderer::Painting::sample(float x, float y) const noexcept
{
    // Stay inside the painting's own frame (about 12 px on every side).
    x = juce::jlimit(13.0f, static_cast<float>(width) - 14.001f, x);
    y = juce::jlimit(11.0f, static_cast<float>(height) - 12.001f, y);
    const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
    const auto& a = pixels[static_cast<size_t>(y0 * width + x0)];
    const auto& b = pixels[static_cast<size_t>(y0 * width + x0 + 1)];
    const auto& c = pixels[static_cast<size_t>((y0 + 1) * width + x0)];
    const auto& d = pixels[static_cast<size_t>((y0 + 1) * width + x0 + 1)];
    auto lerp = [](float p, float q, float t) { return p + (q - p) * t; };
    return { lerp(lerp(a.r, b.r, fx), lerp(c.r, d.r, fx), fy), lerp(lerp(a.g, b.g, fx), lerp(c.g, d.g, fx), fy),
             lerp(lerp(a.b, b.b, fx), lerp(c.b, d.b, fx), fy) };
}

PortraitRenderer::PortraitRenderer()
{
    const char* files[] { "red_seal.png", "purple_seal.png", "ice_seal.png" };
    for (size_t i = 0; i < paintings.size(); ++i)
    {
        const auto image = loadImage(files[i]);
        auto& p = paintings[i];
        p.width = image.getWidth();
        p.height = image.getHeight();
        p.pixels.resize(static_cast<size_t>(p.width * p.height));
        const juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < p.height; ++y)
            for (int x = 0; x < p.width; ++x)
            {
                const auto c = data.getPixelColour(x, y);
                p.pixels[static_cast<size_t>(y * p.width + x)] = { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() };
            }
    }
}

// Landmarks measured on the approved portraits (source pixel coordinates).
const PortraitRenderer::Landmarks& PortraitRenderer::landmarksFor(int character) noexcept
{
    static const Landmarks red    { { { { { 86, 57 }, 11, 6 }, { { 147, 58 }, 8, 6 } } }, { 124, 66 }, { 124, 94 }, 27, { { { 103, 80 }, { 147, 80 } } }, { 101, 190 } };
    static const Landmarks purple { { { { { 86, 51 }, 11, 6 }, { { 146, 51 }, 8, 6 } } }, { 121, 60 }, { 121, 87 }, 28, { { { 100, 74 }, { 144, 74 } } }, { 104, 190 } };
    static const Landmarks ice    { { { { { 84, 55 }, 11, 6 }, { { 145, 54 }, 8, 6 } } }, { 118, 62 }, { 120, 89 }, 28, { { { 98, 76 }, { 142, 76 } } }, { 104, 190 } };
    return character == 0 ? red : character == 2 ? ice : purple;
}

// Colour of the expressive painting at a painting-space position. Liquify-style inverse
// mapping: head tilt, mouth (open jaw or manic grin), brows, eyes, whisker pads.
PortraitRenderer::Rgb PortraitRenderer::expressive(const Painting& src, const Landmarks& lm, const Expression& ex,
                                                   float x, float y) const noexcept
{
    const auto& f = ex.face;
    const float grin = ex.grin;
    auto mix = [](Rgb a, Rgb b, float t) { return Rgb { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; };
    auto mul = [](Rgb a, float k) { return Rgb { a.r * k, a.g * k, a.b * k }; };
    auto hex = [](uint32_t v) { return Rgb { ((v >> 16) & 0xff) / 255.0f, ((v >> 8) & 0xff) / 255.0f, (v & 0xff) / 255.0f }; };

    // Undo the head: tilt back about the neck and rise with pitch lift.
    const float angle = 0.10f * f.headLift, lift = 7.0f * f.headLift;
    const float ca = std::cos(angle), sa = std::sin(angle);
    const float qx = x - lm.pivot.x, qy = y + lift - lm.pivot.y;
    float sx = ca * qx - sa * qy + lm.pivot.x, sy = sa * qx + ca * qy + lm.pivot.y;

    int kind = 0;
    float inside = 0.0f, depth = 0.0f, u = 0.0f, edgeTop = 0.0f;
    if (grin > 0.01f)
    {
        // Hike the cheeks and lip corners by warping the fur, then open strictly below the painted lip.
        const float half = lm.mouthHalf * 1.12f;
        for (float side : { -1.0f, 1.0f })
        {
            const float dx = sx - (lm.mouth.x + side * half * 0.95f), dy = sy - (lm.mouth.y + 8.0f);
            sy += 15.0f * grin * std::exp(-(dx * dx) / (2.0f * 12.0f * 12.0f) - (dy * dy) / (2.0f * 16.0f * 16.0f));
        }
        const float d = (sx - lm.mouth.x) / half;
        if (std::abs(d) < 1.0f)
        {
            const float lipD = (sx - lm.mouth.x) / lm.mouthHalf;
            const float top = lm.mouth.y + 1.5f + 9.0f * lipD * lipD;
            const float h = 10.0f * grin * std::pow(1.0f - d * d, 0.6f);
            const float fade = 1.0f - smoothstep(top + 30.0f, top + 75.0f, sy);
            if (sy > top && sy < top + h)
            {
                kind = 2; u = d; depth = (sy - top) / h; edgeTop = top;
                inside = juce::jmin(smoothstep(0.0f, 1.8f, sy - top), smoothstep(0.0f, 1.8f, top + h - sy));
            }
            else if (sy >= top + h) sy -= h * fade;
        }
    }
    else if (f.mouthOpen > 0.05f)
    {
        // Open mouth: flat upper lip, rounded jaw dropping as one piece.
        const float half = lm.mouthHalf * (0.45f + 0.4f * f.jawWidth) * (1.0f - 0.35f * f.mouthRound);
        const float big = (20.0f + 8.0f * f.mouthRound) * f.mouthOpen;
        const auto opening = [&](float px) { const float dd = (px - lm.mouth.x) / half; return std::abs(dd) < 1.0f ? big * std::sqrt(1.0f - dd * dd) : 0.0f; };
        const float d = (sx - lm.mouth.x) / half;
        const float top = lm.mouth.y + 1.5f * d * d - (std::abs(d) < 1.0f ? f.mouthRound * 0.25f * big * std::sqrt(1.0f - d * d) : 0.0f);
        const float h = opening(sx);
        float shift = 0.0f;
        for (int k = -3; k <= 3; ++k) shift += opening(sx + static_cast<float>(k) * 2.0f);
        shift = juce::jmax(h, shift / 7.0f);
        const float fade = 1.0f - smoothstep(top + 35.0f, top + 85.0f, sy);
        if (h > 0.0f && sy > top && sy < top + h)
        {
            kind = 1; u = d; depth = (sy - top) / h; edgeTop = top;
            inside = juce::jmin(smoothstep(0.0f, 0.8f, sy - top), smoothstep(0.0f, 0.8f, top + h - sy));
        }
        else if (sy >= top + h && std::abs(d) < 1.8f) sy -= shift * fade;
    }
    const float faceY = sy;

    if (grin > 0.01f)
        for (const auto& e : lm.eyes)
        {
            // Angry brows: inner ends down, outer ends up.
            const float inner = lm.mouth.x > e.centre.x ? 1.0f : -1.0f;
            const float dx1 = sx - (e.centre.x + inner * e.rx * 0.6f), dy1 = sy - (e.centre.y - e.ry * 1.9f);
            const float dx2 = sx - (e.centre.x - inner * e.rx * 0.9f), dy2 = sy - (e.centre.y - e.ry * 1.8f);
            sy -= 8.0f * grin * std::exp(-(dx1 * dx1 + dy1 * dy1) / (2.0f * 7.0f * 7.0f));
            sy += 3.5f * grin * std::exp(-(dx2 * dx2 + dy2 * dy2) / (2.0f * 7.0f * 7.0f));
        }
    const float eyeScale = grin > 0.01f ? 1.0f + 0.3f * grin
                                        : juce::jlimit(0.12f, 1.35f, 0.8f + 0.5f * f.eyeOpen - 0.8f * f.eyeSquint);
    for (const auto& e : lm.eyes)
    {
        const float du = (sx - e.centre.x) / (e.rx * 1.9f), dv = (sy - e.centre.y) / (e.ry * 2.8f);
        const float dist = du * du + dv * dv;
        if (dist < 6.0f)
            sy = e.centre.y + (sy - e.centre.y) / (1.0f + (eyeScale - 1.0f) * std::exp(-dist * 1.6f));
    }
    const float puff = 0.22f * juce::jmax(f.throatTension, grin * 0.8f);
    if (puff > 0.0f)
        for (const auto& pad : lm.pads)
        {
            const float dx = sx - pad.x, dy = sy - pad.y;
            if (dx * dx + dy * dy > 45.0f * 45.0f) continue;
            const float k = 1.0f + puff * std::exp(-(dx * dx + dy * dy) / (2.0f * 13.0f * 13.0f));
            sx = pad.x + dx / k;
            sy = pad.y + dy / k;
        }

    Rgb c = src.sample(sx, inside > 0.0f ? (depth < 0.5f ? edgeTop - 0.5f : edgeTop + 0.5f) : sy);
    if (kind == 0 && (f.mouthOpen > 0.05f || grin > 0.01f))
    {
        const float lip = lm.mouth.y;
        if (faceY < lip + 1.0f && faceY > lip - 4.0f && std::abs(sx - lm.mouth.x) < lm.mouthHalf)
            c = mul(c, 0.82f + 0.18f * smoothstep(0.0f, 4.0f, lip - faceY));
    }
    if (kind == 1 && inside > 0.0f)
    {
        Rgb m = mix(hex(0x0e0306), hex(0x2c0b12), depth);
        const float tu = u / 0.6f, tv = (depth - 0.86f) / 0.28f;
        const float tongue = juce::jlimit(0.0f, 1.0f, (1.0f - (tu * tu + tv * tv)) * 3.0f);
        m = mix(m, mix(hex(0x7a3844), hex(0x4e1e28), juce::jlimit(0.0f, 1.0f, std::abs(tu))), tongue);
        if (f.jawWidth > 0.75f)
            for (float side : { -1.0f, 1.0f })
            {
                const float fu = std::abs(u - side * 0.55f), tip = 0.30f;
                if (depth < tip && fu < 0.09f * (1.0f - depth / tip))
                    m = mul(hex(0xe2dccb), 0.85f + 0.15f * (1.0f - depth / tip));
            }
        c = mix(mul(c, 0.55f), m, inside);
    }
    if (kind == 2 && inside > 0.0f)
    {
        // Seal teeth in a stretched grin: gums, short conical teeth, small curved fangs.
        const float a = std::asin(juce::jlimit(-1.0f, 1.0f, u)) / juce::MathConstants<float>::halfPi;
        const float gumTop = 0.15f, gumBottom = 0.86f;
        Rgb m = mix(hex(0x0e0608), hex(0x1e0d12), depth);
        {
            const float tu = a / 0.35f, tv = (depth - 0.78f) / 0.16f;
            m = mix(m, hex(0x4a222b), 0.8f * juce::jlimit(0.0f, 1.0f, (1.0f - (tu * tu + tv * tv)) * 2.0f));
        }
        struct Tooth { float centre, halfWidth, length; };
        static const Tooth upper[] { { 0.06f, 0.045f, 0.22f }, { 0.16f, 0.050f, 0.24f }, { 0.31f, 0.072f, 0.42f },
                                     { 0.47f, 0.058f, 0.24f }, { 0.61f, 0.052f, 0.19f }, { 0.74f, 0.043f, 0.14f } };
        static const Tooth lower[] { { 0.07f, 0.042f, 0.18f }, { 0.17f, 0.046f, 0.19f }, { 0.26f, 0.060f, 0.30f },
                                     { 0.42f, 0.048f, 0.11f }, { 0.57f, 0.043f, 0.08f }, { 0.71f, 0.034f, 0.06f } };
        bool hit = false, fang = false;
        float across = 0.0f, along = 0.0f;
        auto testRow = [&](const Tooth* row, bool upperRow)
        {
            for (int i = 0; i < 6 && ! hit; ++i)
                for (float side : { -1.0f, 1.0f })
                {
                    const int id = i * 2 + (side > 0 ? 1 : 0) + (upperRow ? 0 : 20);
                    const float jitter = (hash(id, 7) - 0.5f) * 0.02f;
                    const float len = row[i].length * (0.86f + 0.24f * hash(id, 3));
                    const float t = upperRow ? (depth - gumTop) / len : (gumBottom - depth) / len;
                    if (t < 0.0f || t > 1.0f) continue;
                    const float centre = side * row[i].centre + jitter + (i == 2 ? 0.035f * t * t * side : 0.0f);
                    const float hw = row[i].halfWidth * (1.0f - std::pow(t, 1.6f)) * (1.0f - 0.25f * std::abs(a));
                    const float ac = (a - centre) / juce::jmax(0.002f, hw);
                    if (std::abs(ac) < 1.0f) { hit = true; across = ac; along = t; fang = i == 2; return; }
                }
        };
        testRow(upper, true);
        if (! hit) testRow(lower, false);
        if (hit)
        {
            Rgb enamel = mix(hex(0xa8956c), hex(0xddd3ba), smoothstep(0.0f, 0.6f, along));
            enamel = mul(enamel, (0.62f + 0.38f * std::sqrt(juce::jmax(0.0f, 1.0f - across * across))) * (1.0f - 0.5f * a * a));
            if (fang && across > -0.55f && across < -0.15f && along > 0.2f && along < 0.75f)
                enamel = mix(enamel, hex(0xfbf8ef), 0.55f);
            m = enamel;
        }
        else if (depth < gumTop || depth > gumBottom)
        {
            const float g = depth < gumTop ? depth / gumTop : (1.0f - depth) / (1.0f - gumBottom);
            m = mul(mix(hex(0x3a1a22), hex(0x7a4652), g), 1.0f - 0.45f * a * a);
        }
        m = mul(m, (0.55f + 0.45f * smoothstep(0.0f, 0.3f, depth)) * (1.0f - 0.6f * smoothstep(0.75f, 1.0f, std::abs(a))));
        c = mix(mix(hex(0x2a2427), c, 0.35f), m, smoothstep(0.0f, 1.0f, inside));
    }
    return c;
}

juce::Image PortraitRenderer::render(int character, const Expression& ex, int width, int height) const
{
    const auto& painting = paintings[static_cast<size_t>(juce::jlimit(0, 2, character))];
    const auto& lm = landmarksFor(character);
    const auto& pal = paletteFor(character);
    // Six-step ramp from the desktop sky up to a pale highlight in the character's colours.
    const juce::Colour ramp[] { pal.sky, pal.accent.darker(1.1f), pal.accent.darker(0.45f), pal.accent,
                                pal.accent.interpolatedWith(pal.secondary, 0.55f), pal.secondary.interpolatedWith(juce::Colours::white, 0.6f) };
    static const int bayer[4][4] { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

    // Cover-fit the painting's interior (its own frame cropped away) into the bitmap.
    const juce::Rectangle<float> crop { 14.0f, 12.0f, painting.width - 28.0f, painting.height - 24.0f };
    const float k = juce::jmax(width / crop.getWidth(), height / crop.getHeight());
    const float ox = (width - crop.getWidth() * k) * 0.5f, oy = (height - crop.getHeight() * k) * 0.5f + 2.0f;

    juce::Image out(juce::Image::RGB, width, height, false);
    juce::Image::BitmapData data(out, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const float px = crop.getX() + (static_cast<float>(x) + 0.5f - ox) / k;
            const float py = crop.getY() + (static_cast<float>(y) + 0.5f - oy) / k;
            const auto c = expressive(painting, lm, ex, px, py);
            float lum = 0.3f * c.r + 0.59f * c.g + 0.11f * c.b;
            lum = juce::jlimit(0.0f, 1.0f, (lum - 0.06f) / 0.82f * (1.0f + 0.2f * ex.face.intensity));
            const float v = lum * 5.0f;
            int level = static_cast<int>(std::floor(v));
            if ((v - static_cast<float>(level)) * 16.0f > static_cast<float>(bayer[y % 4][x % 4])) ++level;
            data.setPixelColour(x, y, ramp[juce::jlimit(0, 5, level)]);
        }
    return out;
}
}
