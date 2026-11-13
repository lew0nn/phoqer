// The fish you can feed the seal, and the blue screen it shows when overfed.

#include "Views.h"

#include "../Core/FishSprite.h"

#include "Style.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace phoqer::ui
{
// ----------------------------------------------------------------------------------- FISH
namespace
{
juce::Image drawFish(const char* const* rows, int character)
{
    const auto& pal = paletteFor(character);
    juce::Image image(juce::Image::ARGB, Fish98::spriteW, Fish98::spriteH, true);
    for (int y = 0; y < Fish98::spriteH; ++y)
        for (int x = 0; x < Fish98::spriteW; ++x)
        {
            juce::Colour c;
            switch (rows[y][x])
            {
                case 'X': case 'K': c = juce::Colour(0xff180c14); break;
                case 'o': c = pal.accent; break;
                case 's': c = pal.accent.interpolatedWith(juce::Colours::white, 0.45f); break;
                case 'd': c = pal.accent.darker(0.5f); break;
                case 'W': c = juce::Colours::white; break;
                default: continue;
            }
            image.setPixelAt(x, y, c);
        }
    return image;
}
}

Fish98::Fish98()
{
    setSize(spriteW * pixel, spriteH * pixel);
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setCharacter(0);
}

void Fish98::setCharacter(int character)
{
    frames = { drawFish(phoqer::fishStraight, character), drawFish(phoqer::fishFlicked, character) };
    repaint();
}

void Fish98::setHome(juce::Point<int> topLeft)
{
    home = topLeft;
    if (! held) setTopLeftPosition(home);
}

void Fish98::paint(juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImage(frames[static_cast<size_t>(frame)], getLocalBounds().toFloat());
}

void Fish98::mouseDown(const juce::MouseEvent& e)
{
    held = true;
    returning = false;
    toFront(false);
    dragger.startDraggingComponent(this, e);
}

void Fish98::mouseDrag(const juce::MouseEvent& e)
{
    dragger.dragComponent(this, e, nullptr);
}

void Fish98::mouseUp(const juce::MouseEvent&)
{
    held = false;
    if (onDrop && onDrop(getBounds().getCentre())) { setVisible(false); return; }
    returning = getPosition() != home;
}

void Fish98::visibilityChanged()
{
    if (isVisible()) startTimerHz(30);
    else
    {
        stopTimer();
        held = returning = false;
        setTopLeftPosition(home);
    }
}

// The tail flicks slowly at rest and fast in the hand; let go and the fish eases back home.
void Fish98::timerCallback()
{
    if (++ticks >= (held ? 3 : 9))
    {
        ticks = 0;
        frame ^= 1;
        repaint();
    }
    if (returning)
    {
        const auto at = getPosition().toFloat(), to = home.toFloat();
        const auto next = at + (to - at) * 0.225f;
        if (next.getDistanceFrom(to) < 1.0f) { setTopLeftPosition(home); returning = false; }
        else setTopLeftPosition(next.roundToInt());
    }
}

// ----------------------------------------------------------------------------------- BLUE SCREEN
BlueScreen98::BlueScreen98()
{
    setWantsKeyboardFocus(false);
    startTimer(530);
}

void BlueScreen98::paint(juce::Graphics& g)
{
    const juce::Colour blue { 0xff0000aa }, grey { 0xffaaaaaa };
    g.fillAll(blue);
    const auto font = pixelFont(12.0f);
    const float lineH = 22.0f, left = 70.0f, width = static_cast<float>(getWidth()) - 2.0f * left;
    float y = std::round(static_cast<float>(getHeight()) * 0.5f - 6.5f * lineH);

    const juce::String title = " SEAL.SYS ";
    const float titleW = 120.0f, titleX = std::round((static_cast<float>(getWidth()) - titleW) * 0.5f);
    g.setColour(grey);
    g.fillRect(titleX, y - 3.0f, titleW, lineH);
    drawText(g, title, { titleX, y - 3.0f, titleW, lineH }, font, blue, juce::Justification::centred);
    y += 2.0f * lineH;

    const char* lines[] { "A FATAL EXCEPTION 0F (FISH) HAS OCCURRED AT 0028:C0FFEE15 IN",
                          "VXD SEAL(05) + 0000FEED. THE SEAL HAS EATEN TOO MUCH FISH.",
                          "",
                          "*  PRESS ANY KEY TO REBOOT THE SEAL.",
                          "*  FEED IT AGAIN AND IT WILL BURP AGAIN.",
                          "   YOU WILL LOSE ANY UNDIGESTED FISH." };
    for (const auto* line : lines)
    {
        drawText(g, line, { left, y, width, lineH }, font, juce::Colours::white);
        y += lineH;
    }
    y += lineH;
    drawText(g, juce::String("PRESS ANY KEY TO CONTINUE ") + (cursorOn ? "_" : " "), { left, y, width, lineH }, font,
             juce::Colours::white, juce::Justification::centred);
}
}
