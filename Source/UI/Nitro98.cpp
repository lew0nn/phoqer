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

void ArpSteps98::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    sunken(g, r, juce::Colour(16, 6, 29));
    const auto grid = r.reduced(2.0f);
    const int p = pattern ? pattern() : 0, oct = juce::jlimit(1, 3, octaves ? octaves() : 1);
    std::vector<int> seq;     // C-E-G as rows 0, 1, 2, an octave up 3, 4, 5, ...
    for (int o = 0; o < oct; ++o) for (int k = 0; k < 3; ++k) seq.push_back(o * 3 + k);
    if (p == 1) std::reverse(seq.begin(), seq.end());
    if (p == 2 && seq.size() > 2) for (int i = static_cast<int>(seq.size()) - 2; i >= 1; --i) seq.push_back(seq[static_cast<size_t>(i)]);
    if (p == 3) seq = { 0, 4, 1, 3, 5, 2, 4, 0, 3, 1, 5, 2 };
    if (p == 4) seq = { 1, 0, 2 };     // AS PLAYED: E, C, G as an example order
    const int rows = 3 * oct;
    const float cw = grid.getWidth() / steps, rh = grid.getHeight() / static_cast<float>(rows);
    g.setColour(juce::Colour(52, 30, 90));
    for (int c = 1; c < steps; ++c) g.fillRect(std::round(grid.getX() + c * cw), grid.getY(), 1.0f, grid.getHeight());
    for (int row = 1; row < rows; ++row) g.fillRect(grid.getX(), std::round(grid.getY() + row * rh), grid.getWidth(), 1.0f);
    for (int c = 0; c < steps; ++c)
    {
        const int row = seq[static_cast<size_t>(c) % seq.size()] % rows;
        const juce::Rectangle<float> cell { std::round(grid.getX() + c * cw) + 2.0f, std::round(grid.getBottom() - (row + 1) * rh) + 2.0f,
                                            std::round(cw * 0.8f) - 2.0f, std::round(rh) - 3.0f };
        g.setColour(juce::Colour(150, 96, 255));
        g.fillRect(cell);
    }
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
