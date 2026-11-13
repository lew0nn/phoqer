#pragma once
#include "UI/Style.h"
namespace phoqer::ui {
inline juce::Rectangle<float> legacyWindow98(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
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
    if(palette.accent==juce::Colour(150,96,255))
    {
        const auto& source=paletteFor(2);
        const auto sourceFrom=active?source.accent.darker(.35f):source.accent.darker(1.3f);
        const auto sourceTo=active?source.secondary.darker(.15f):source.secondary.darker(1.6f);
        for(int x=0;x<static_cast<int>(bar.getWidth());++x)
        {
            const auto original=sourceFrom.interpolatedWith(sourceTo,static_cast<float>(x)/juce::jmax(1.0f,bar.getWidth()-1));
            const auto mapped=approvedHoloColour(original);
            g.setColour(mapped);g.fillRect(bar.getX()+x,bar.getY(),1.0f,bar.getHeight());
        }
    }
    else g.fillRect(bar);
    drawText(g, title, bar.reduced(4.0f, 0.0f), pixelFont(titleHeight >= 20.0f ? 12.0f : 10.0f, true), juce::Colours::white);
    return windowClient(r, titleHeight);
}

}
