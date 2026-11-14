#include "Nitro98.h"
#include "Style.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace phoqer::ui
{
juce::Colour nitroNoteColour(int slot) noexcept
{
    const juce::Colour colours[] { juce::Colour(0xffff6db8), juce::Colour(0xff9660ff), juce::Colour(0xff46beb4) };     // pink, violet, teal
    return colours[juce::jlimit(0, 2, slot)];
}

namespace
{
// 9 x 7 and 9 x 8 pixel icons, one UI unit per pixel.
const char* const lips[] { "  ## ##  ", " ####### ", "#########", "         ", "#########", " ####### ", "  #####  " };
const char* const bolt[] { "    #####", "   ####  ", "  ####   ", " ########", "    ###  ", "   ###   ", "  ##     ", "  #      " };
const char* const notes[] { "      ##", "      ##", "   ## ##", "   ## # ", "## #  # ", "## #  # ", "#  #    ", "#       " };     // 8 x 8: notes climbing
}

IconToggle98::IconToggle98(Icon i) : juce::Button(i == Icon::lips ? "YAP" : i == Icon::notes ? "ARP" : "NITRO"), icon(i)
{
    setClickingTogglesState(true);
}

void IconToggle98::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick && isEnabled()) onRightClick(); return; }
    juce::Button::mouseDown(e);
}

void IconToggle98::mouseUp(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    juce::Button::mouseUp(e);
}

void IconToggle98::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto r = getLocalBounds().toFloat();
    const bool on = getToggleState() && isEnabled();
    button98(g, r, on || down);
    const auto glass = r.reduced(3.0f);
    g.setColour(on ? juce::Colour(49, 30, 84) : juce::Colour(16, 6, 29));
    g.fillRect(glass);
    const auto ink = ! isEnabled() ? juce::Colour(70, 64, 78) : on ? juce::Colour(255, 109, 184) : juce::Colour(110, 69, 187);
    const auto rows = icon == Icon::lips ? lips : icon == Icon::notes ? notes : bolt;
    const int rowCount = icon == Icon::lips ? 7 : 8, columns = static_cast<int>(std::strlen(rows[0]));
    const float x0 = std::round(glass.getCentreX() - columns * 0.5f), y0 = std::round(glass.getCentreY() - rowCount * 0.5f);
    g.setColour(ink);
    for (int y = 0; y < rowCount; ++y)
        for (int x = 0; x < columns; ++x)
            if (rows[y][x] == '#') g.fillRect(x0 + x, y0 + y, 1.0f, 1.0f);
}

NitroKeys98::NitroKeys98(std::array<juce::RangedAudioParameter*, 3> n) : notes(n) {}

namespace
{
bool isBlack(int offset) { const int k = ((offset % 12) + 12) % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
int whitesBefore(int offset)
{
    int n = 0;
    for (int o = NitroKeys98::lowest; o < offset; ++o) if (! isBlack(o)) ++n;
    return n;
}
int whiteCount() { return whitesBefore(NitroKeys98::highest + 1); }
int noteValue(juce::RangedAudioParameter* p) { return juce::roundToInt(p->convertFrom0to1(p->getValue())); }
}

float NitroKeys98::whiteWidth() const { return std::floor(static_cast<float>(getWidth()) / static_cast<float>(whiteCount())); }
int NitroKeys98::keyX(int offset) const { return juce::roundToInt(whitesBefore(offset) * whiteWidth()); }

void NitroKeys98::paint(juce::Graphics& g)
{
    const float w = whiteWidth(), h = static_cast<float>(getHeight());
    const auto colourOf = [this](int offset)
    {
        if (offset == 0) return nitroRootColour;
        for (int k = 0; k < 3; ++k) if (noteValue(notes[static_cast<size_t>(k)]) == offset) return nitroNoteColour(k);
        return juce::Colour();
    };
    for (int o = lowest; o <= highest; ++o)
        if (! isBlack(o))
        {
            const juce::Rectangle<float> key { whitesBefore(o) * w, 0.0f, w, h };
            const auto c = colourOf(o);
            g.setColour(c.isTransparent() ? juce::Colours::white : c);
            g.fillRect(key);
            g.setColour(juce::Colours::black);
            g.drawRect(key.withWidth(w + 1.0f), 1.0f);
        }
    for (int o = lowest; o <= highest; ++o)
        if (isBlack(o))
        {
            const auto c = colourOf(o);
            const juce::Rectangle<float> key { whitesBefore(o) * w - std::round(w * 0.32f), 0.0f, std::round(w * 0.64f), std::round(h * 0.6f) };
            g.setColour(juce::Colours::black);
            g.fillRect(key);
            if (! c.isTransparent()) { g.setColour(c); g.fillRect(key.reduced(1.0f)); }
        }
}

int NitroKeys98::offsetAt(juce::Point<float> p) const
{
    const float w = whiteWidth(), h = static_cast<float>(getHeight());
    if (p.y < std::round(h * 0.6f))
        for (int o = lowest; o <= highest; ++o)
            if (isBlack(o))
            {
                const float x = whitesBefore(o) * w - std::round(w * 0.32f);
                if (p.x >= x && p.x < x + std::round(w * 0.64f)) return o;
            }
    for (int o = lowest; o <= highest; ++o)
        if (! isBlack(o) && p.x >= whitesBefore(o) * w && p.x < (whitesBefore(o) + 1) * w) return o;
    return 0;
}

void NitroKeys98::mouseDown(const juce::MouseEvent& e)
{
    const int offset = offsetAt(e.position);
    if (offset == 0) return;     // the key itself always plays
    const auto set = [](juce::RangedAudioParameter* p, int v)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(v)));
        p->endChangeGesture();
    };
    for (auto* p : notes) if (noteValue(p) == offset) { set(p, 0); repaint(); return; }     // tapped again: taken away
    for (auto* p : notes) if (noteValue(p) == 0) { set(p, offset); repaint(); return; }     // into the first empty slot
    set(notes[2], offset);                                                                   // all three in use: the last one moves
    repaint();
}

void SegmentButton98::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    button98(g, r, on || down);     // PHOQER's own pressed look; the text stays still
    drawText(g, getButtonText(), r, pixelFont(8.0f, true), on ? juce::Colour(90, 40, 170) : juce::Colours::black, juce::Justification::centred);
}

namespace
{
constexpr int lanePad = 4, laneGap = 3;     // inside the sunken frame: the same padding on every side, 3 px between steps
const juce::Colour laneGlass { 16, 6, 29 }, laneShade { 26, 14, 46 }, laneBar { 150, 96, 255 }, lanePlaying { 255, 109, 184 },
                   laneHover { 196, 160, 255 }, laneRest { 60, 40, 96 };
}

juce::Rectangle<int> ArpLanes98::cell(int step) const
{
    const auto inner = getLocalBounds().reduced(2 + lanePad);
    const int w = (inner.getWidth() - laneGap * (steps - 1)) / steps, used = w * steps + laneGap * (steps - 1);
    const int left = inner.getX() + (inner.getWidth() - used) / 2;     // the leftover split evenly: centred
    return { left + step * (w + laneGap), inner.getY(), w, inner.getHeight() };
}

int ArpLanes98::stepAt(int x) const
{
    const auto first = cell(0);
    return juce::jlimit(0, steps - 1, (x - first.getX() + laneGap / 2) / (first.getWidth() + laneGap));
}

void ArpLanes98::paint(juce::Graphics& g)
{
    sunken(g, getLocalBounds().toFloat(), laneGlass);
    const auto inner = getLocalBounds().reduced(2);
    g.setColour(laneShade);     // every other beat shaded, framing whole steps
    for (int beat = 1; beat < steps / 4; beat += 2)
    {
        const auto a = cell(beat * 4), b = cell(beat * 4 + 3);
        g.fillRect(juce::Rectangle<int>::leftTopRightBottom(a.getX() - laneGap / 2 - 1, inner.getY(), b.getRight() + laneGap / 2 + 1, inner.getBottom()));
    }
    const int now = playing ? playing() : -1;
    for (int i = 0; i < steps; ++i)
    {
        const auto c = cell(i);
        const auto step = get ? get(i) : phoqer::ArpLaneStep {};
        if (step.rest)
        {
            g.setColour(laneRest);
            g.drawRect(c.withTop(c.getBottom() - 3), 1);
            continue;
        }
        const float value = gateMode ? step.gate : step.velocity;
        const int h = juce::jmax(2, juce::roundToInt(static_cast<float>(c.getHeight()) * value));
        g.setColour(i == now ? lanePlaying : (i == hover ? laneHover : laneBar));
        g.fillRect(c.withTop(c.getBottom() - h));
    }
    if (hover >= 0 && (dragging || isMouseOver()))     // a Win98 tooltip with the value
    {
        const auto step = get ? get(hover) : phoqer::ArpLaneStep {};
        const auto text = step.rest ? juce::String("REST")
                                    : juce::String(gateMode ? "GATE " : "VEL ") + juce::String(juce::roundToInt((gateMode ? step.gate : step.velocity) * 100.0f)) + "%";
        const auto font = pixelFont(8.0f);
        const int w = juce::roundToInt(font.getStringWidthFloat(text)) + 8;
        const int x = juce::jlimit(inner.getX(), inner.getRight() - w, cell(hover).getX() - 2);
        const juce::Rectangle<int> tip { x, inner.getY() + 1, w, 11 };
        g.setColour(juce::Colour(255, 255, 225)); g.fillRect(tip);
        g.setColour(juce::Colours::black); g.drawRect(tip, 1);
        drawText(g, text, tip.toFloat(), font, juce::Colours::black, juce::Justification::centred);
    }
}

void ArpLanes98::draw(const juce::MouseEvent& e)
{
    const int i = stepAt(e.x);
    const auto c = cell(i);
    const float value = juce::jlimit(0.05f, 1.0f, std::round(static_cast<float>(c.getBottom() - e.y) / static_cast<float>(c.getHeight()) * 100.0f) / 100.0f);
    auto step = get ? get(i) : phoqer::ArpLaneStep {};
    (gateMode ? step.gate : step.velocity) = value;
    step.rest = false;     // drawing on a rest brings the step back
    if (set) set(i, step);
    hover = i;
    repaint();
}

void ArpLanes98::mouseMove(const juce::MouseEvent& e)
{
    if (const int i = stepAt(e.x); i != hover) { hover = i; repaint(); }
}

void ArpLanes98::mouseExit(const juce::MouseEvent&)
{
    if (! dragging) { hover = -1; repaint(); }
}

void ArpLanes98::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())     // right-click: a rest, or the step back
    {
        const int i = stepAt(e.x);
        auto step = get ? get(i) : phoqer::ArpLaneStep {};
        step.rest = ! step.rest;
        if (set) set(i, step);
        if (edited) edited();
        hover = i;
        repaint();
        return;
    }
    dragging = true;
    draw(e);
}

void ArpLanes98::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging) draw(e);
}

void ArpLanes98::mouseUp(const juce::MouseEvent& e)
{
    if (! dragging) return;
    dragging = false;
    if (edited) edited();
    if (! isMouseOver()) hover = -1;
    repaint();
    juce::ignoreUnused(e);
}

void ArpLanes98::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const int i = stepAt(e.x);
    auto step = get ? get(i) : phoqer::ArpLaneStep {};
    (gateMode ? step.gate : step.velocity) = 1.0f;
    step.rest = false;
    if (set) set(i, step);
    if (edited) edited();
    repaint();
}

void LevelSlider98::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float cy = std::round(r.getCentreY());
    const auto track = juce::Rectangle<float>(r.getX() + 4.0f, cy - 3.0f, r.getWidth() - 8.0f, 7.0f);
    sunken(g, track, juce::Colour(40, 26, 60));
    const float x = track.getX() + 2.0f + std::round((track.getWidth() - 4.0f) * static_cast<float>(valueToProportionOfLength(getValue())));
    g.setColour(colour);
    g.fillRect(juce::Rectangle<float>(track.getX() + 2.0f, cy - 1.0f, x - track.getX() - 2.0f, 3.0f));
    button98(g, { x - 4.0f, cy - 7.0f, 9.0f, 14.0f }, false);
}
}
