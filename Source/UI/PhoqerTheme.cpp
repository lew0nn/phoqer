#include "PhoqerTheme.h"
#include <PhoqerAssets.h>
#include <algorithm>
namespace phoqer::ui
{
namespace
{
juce::Image asset(const char* name)
{
    int size = 0;
    const auto* data = PhoqerAssets::getNamedResource(name, size);
    auto image = juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(size));
    jassert(image.isValid());
    return image;
}
ThemeAssets makeTheme(const juce::String& color, juce::Colour accent, juce::Colour accentBright,
    juce::Colour accentSoft, juce::Colour text, juce::Colour textMuted, juce::Colour display,
    juce::Colour shellTop, juce::Colour shellBottom, juce::Colour panelTop,
    juce::Colour panelBottom, juce::Colour metalLight, juce::Colour metalDark, juce::Colour hairline)
{
    const auto get = [&color](const char* prefix) { return asset((juce::String(prefix) + color + "_png").toRawUTF8()); };
    return { asset((color + "_seal_png").toRawUTF8()), get("knob_body_"),
        get("tide_body_"), get("tide_track_"), get("tide_fill_"), get("tide_handle_"),
        get("meter_off_"), get("meter_on_"), get("wave_frame_"), get("led_"), asset("menu_icon_png"),
        {{ asset("button_voice_png"), asset("button_formant_png"), asset("button_harmonics_png"),
           asset("button_space_png"), asset("button_bite_png") }},
        accent, accentBright, accentSoft, text, textMuted, display, shellTop, shellBottom,
        panelTop, panelBottom, metalLight, metalDark, hairline };
}
}
ThemeStore::ThemeStore() : themes {{
    makeTheme("purple",
            juce::Colour(0xff9d43f2), juce::Colour(0xffd9b6ff), juce::Colour(0xff5f35a4),
            juce::Colour(0xffeee8f8), juce::Colour(0xffaaa0bd), juce::Colour(0xff070811),
            juce::Colour(0xff202039), juce::Colour(0xff0d0e1d), juce::Colour(0xff18192c),
            juce::Colour(0xff090a15), juce::Colour(0xffa0a3b2), juce::Colour(0xff171820),
            juce::Colour(0xff6e688e)),
    makeTheme("ice",
            juce::Colour(0xff238df9), juce::Colour(0xffc8e7ff), juce::Colour(0xff2f69b4),
            juce::Colour(0xff172436), juce::Colour(0xff53657a), juce::Colour(0xff050a12),
            juce::Colour(0xffe4edf4), juce::Colour(0xff8797a7), juce::Colour(0xffcad7e1),
            juce::Colour(0xff687989), juce::Colour(0xffeef4f8), juce::Colour(0xff303b45),
            juce::Colour(0xff526d87)),
    makeTheme("red",
            juce::Colour(0xffee3731), juce::Colour(0xffff9a8f), juce::Colour(0xff962823),
            juce::Colour(0xffefe2df), juce::Colour(0xffa98b89), juce::Colour(0xff0c0608),
            juce::Colour(0xff2a2527), juce::Colour(0xff111012), juce::Colour(0xff211e20),
            juce::Colour(0xff0c0c0e), juce::Colour(0xff999696), juce::Colour(0xff181719),
            juce::Colour(0xff765e60))
}} {}
VisualStyle visualStyleForCharacter(int characterIndex) noexcept
{
    switch (characterIndex)
    {
        case 0: return VisualStyle::red;
        case 2: return VisualStyle::ice;
        case 1:
        default: return VisualStyle::purple;
    }
}

const ThemeAssets& ThemeStore::get(VisualStyle id) const noexcept
{
    return get(static_cast<int>(id));
}

const ThemeAssets& ThemeStore::get(int id) const noexcept
{
    return themes[static_cast<size_t>(std::clamp(id, 0, 2))];
}
}
