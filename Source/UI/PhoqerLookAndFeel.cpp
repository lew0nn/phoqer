#include "PhoqerLookAndFeel.h"
namespace phoqer::ui
{
PhoqerLookAndFeel::PhoqerLookAndFeel()
{
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0b0c13));
    setColour(juce::PopupMenu::textColourId, juce::Colours::white);
}
void PhoqerLookAndFeel::setTheme(const ThemeAssets& value) noexcept
{
    theme = &value;
    setColour(juce::PopupMenu::highlightedBackgroundColourId, theme->accentSoft);
    setColour(juce::PopupMenu::highlightedTextColourId, theme->text);
}
void PhoqerLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width,
    int height, float position, float startAngle, float endAngle, juce::Slider&)
{
    if (theme == nullptr) return;
    const float diameter = juce::jmin(108.0f, static_cast<float>(juce::jmin(width, height)) - 4.0f);
    const auto art = juce::Rectangle<float>(diameter, diameter).withCentre(
        { x + width * 0.5f, y + height * 0.5f });
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.setOpacity(1.0f);
    g.drawImage(theme->knobBody, art);
    const auto centre = art.getCentre();
    const auto angle = startAngle + position * (endAngle - startAngle);
    if (position > 0.0f)
    {
        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, diameter * 0.435f, diameter * 0.435f,
            0.0f, startAngle, angle, true);
        g.setColour(theme->accent.withAlpha(0.18f));
        g.strokePath(arc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(theme->accent);
        g.strokePath(arc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // Source inner cap radius is approximately .33 of body diameter.
    // The only value pointer occupies 68-82 percent of that cap radius.
    const auto direction = juce::Point<float>(std::sin(angle), -std::cos(angle));
    juce::Path pointer;
    pointer.startNewSubPath(centre + direction * (diameter * 0.33f * 0.68f));
    pointer.lineTo(centre + direction * (diameter * 0.33f * 0.82f));
    g.setColour(juce::Colours::white.interpolatedWith(theme->accentBright, 0.20f));
    g.strokePath(pointer, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
void PhoqerLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width,
    int height, float, float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (theme == nullptr || style != juce::Slider::LinearVertical) return;
    const auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
        static_cast<float>(width), static_cast<float>(height));
    const float scale = juce::jmin(area.getWidth() / theme->tideBody.getWidth(),
                                  area.getHeight() / theme->tideBody.getHeight());
    const auto body = juce::Rectangle<float>(theme->tideBody.getWidth() * scale,
        theme->tideBody.getHeight() * scale).withCentre(area.getCentre());
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.setOpacity(1.0f);
    g.drawImage(theme->tideBody, body);
    const auto track = juce::Rectangle<float>(body.getWidth() * 0.28f, body.getHeight() * 0.79f)
        .withCentre({ body.getCentreX(), body.getCentreY() + body.getHeight() * 0.015f });
    // Tile the original dark track material at uniform scale.
    const float tileHeight = track.getWidth() * theme->tideTrack.getHeight() / theme->tideTrack.getWidth();
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(track.toNearestInt());
        for (float ty = track.getY(); ty < track.getBottom(); ty += tileHeight)
            g.drawImage(theme->tideTrack, {track.getX(), ty, track.getWidth(), tileHeight});
    }
    const float value = static_cast<float>(slider.valueToProportionOfLength(slider.getValue()));
    const float thumbY = track.getBottom() - value * track.getHeight();
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(juce::Rectangle<float>(track.getX(), thumbY, track.getWidth(),
            track.getBottom() - thumbY).toNearestInt());
        // A fixed, aspect-preserving fill image; reveal by clipping, never stretch with value.
        g.drawImage(theme->tideFill, track, juce::RectanglePlacement::fillDestination);
    }
    const float handleWidth = body.getWidth() * 0.54f;
    const float handleHeight = handleWidth * theme->tideHandle.getHeight() / theme->tideHandle.getWidth();
    g.drawImage(theme->tideHandle, juce::Rectangle<float>(handleWidth, handleHeight)
        .withCentre({body.getCentreX(), thumbY}));
}
void PhoqerLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    if (theme == nullptr)
        return;
    g.fillAll(theme->panelBottom);
    g.setColour(theme->hairline);
    g.drawRect(0, 0, width, height);
}

void PhoqerLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool separator, bool active, bool highlighted,
                                          bool ticked, bool hasSubMenu, const juce::String& text,
                                          const juce::String& shortcut, const juce::Drawable*,
                                          const juce::Colour*)
{
    if (theme == nullptr)
        return;
    if (separator)
    {
        g.setColour(theme->hairline.withAlpha(0.5f));
        g.drawHorizontalLine(area.getCentreY(), static_cast<float>(area.getX() + 8),
                             static_cast<float>(area.getRight() - 8));
        return;
    }
    if (highlighted && active)
    {
        g.setColour(theme->accentSoft.withAlpha(0.7f));
        g.fillRect(area.reduced(2));
    }
    g.setColour(active ? theme->text : theme->textMuted.withAlpha(0.5f));
    g.setFont(juce::FontOptions(12.0f));
    auto textArea = area.reduced(12, 0);
    if (ticked)
        g.fillEllipse(static_cast<float>(textArea.getX()), static_cast<float>(textArea.getCentreY() - 2), 4.0f, 4.0f);
    g.drawFittedText(text, textArea.withTrimmedLeft(10), juce::Justification::centredLeft, 1);
    g.drawFittedText(shortcut, textArea, juce::Justification::centredRight, 1);
    if (hasSubMenu)
        g.drawText(">", textArea.removeFromRight(10), juce::Justification::centred);
}
}
