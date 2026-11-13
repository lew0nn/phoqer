#include "Style.h"
#include "VoiceControls.h"
#include <cmath>

#include <PhoqerUIAssets.h>

namespace phoqer::ui
{
namespace
{
juce::Typeface::Ptr loadTypeface(const char* originalFilename)
{
    for (int i = 0; i < PhoqerUIAssets::namedResourceListSize; ++i)
    {
        const auto* name = PhoqerUIAssets::namedResourceList[i];
        if (juce::String(PhoqerUIAssets::getNamedResourceOriginalFilename(name)) == originalFilename)
        {
            int size = 0;
            const auto* data = PhoqerUIAssets::getNamedResource(name, size);
            return juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size));
        }
    }
    jassertfalse;
    return {};
}
}

const Palette& paletteFor(int character) noexcept
{
    static const Palette burp { juce::Colour(0xffff2d55), juce::Colour(0xffff9f1c), juce::Colour(0xff12040a) };
    static const Palette squeal { juce::Colour(0xffe04bff), juce::Colour(0xff33e0ff), juce::Colour(0xff07031a) };
    static const Palette groan { juce::Colour(0xff2388b5), juce::Colour(0xff44c27f), juce::Colour(0xff021014) };     // ocean blue to green
    static const Palette kyrax { juce::Colour(150,96,255), juce::Colour(240,100,210), juce::Colour(16,6,29) };
    return character == 3 ? kyrax : character == 0 ? burp : character == 2 ? groan : squeal;
}

// Evenly spaced colours blended in OKLab, so two neighbours never meet in a muddy middle.
juce::Colour oklabRamp(const juce::uint32* stops,int count,float t) noexcept
{
    const auto lin=[](float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);};
    const auto srgb=[](float v){v=juce::jlimit(0.0f,1.0f,v);return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.0f/2.4f)-.055f;};
    const auto toLab=[&](juce::Colour c,float* o)
    {
        const float r=lin(c.getFloatRed()),g=lin(c.getFloatGreen()),b=lin(c.getFloatBlue());
        const float l=std::cbrt(.4122214708f*r+.5363325363f*g+.0514459929f*b),m=std::cbrt(.2119034982f*r+.6806995451f*g+.1073969566f*b),
                    s=std::cbrt(.0883024619f*r+.2817188376f*g+.6299787005f*b);
        o[0]=.2104542553f*l+.7936177850f*m-.0040720468f*s;o[1]=1.9779984951f*l-2.4285922050f*m+.4505937099f*s;o[2]=.0259040371f*l+.7827717662f*m-.8086757660f*s;
    };
    const int n=count-1;
    t=juce::jlimit(0.0f,1.0f,t);
    const int i=juce::jmin(static_cast<int>(t*n),n-1);const float u=t*n-i;
    float a[3],b[3];toLab(juce::Colour(stops[i]),a);toLab(juce::Colour(stops[i+1]),b);
    const float L=a[0]+(b[0]-a[0])*u,A=a[1]+(b[1]-a[1])*u,B=a[2]+(b[2]-a[2])*u;
    const float l=std::pow(L+.3963377774f*A+.2158037573f*B,3.0f),m=std::pow(L-.1055613458f*A-.0638541728f*B,3.0f),s=std::pow(L-.0894841775f*A-1.2914855480f*B,3.0f);
    return juce::Colour::fromFloatRGBA(srgb(4.0767416621f*l-3.3077115913f*m+.2309699292f*s),srgb(-1.2684380046f*l+2.6097574011f*m-.3413193965f*s),
                                       srgb(-.0041960863f*l-.7034186147f*m+1.7076147010f*s),1);
}
juce::Colour oklabRamp(std::initializer_list<juce::uint32> stops,float t) noexcept
{
    return oklabRamp(stops.begin(),static_cast<int>(stops.size()),t);
}
// KYRAX: violet to rose (knob arcs), and on to cream (the top title bar).
juce::Colour kyraxRamp(float t) noexcept
{
    return oklabRamp(kyraxStops,kyraxRose+1,t);
}
juce::Colour kyraxTitle(float t) noexcept     // the cream holds from 85% of the bar to its end
{
    return oklabRamp(kyraxStops,kyraxCream+1,t/.85f);
}

juce::Colour themeColour(int voice,float p) noexcept
{
    p=juce::jlimit(0.0f,1.0f,p);
    const juce::Colour from[]={juce::Colour(188,34,61),juce::Colour(163,57,188),juce::Colour(25,101,134),juce::Colour(80,30,220)};
    const juce::Colour to[]={juce::Colour(217,127,28),juce::Colour(57,179,217),juce::Colour(55,161,113),juce::Colour(250,236,50)};
    voice=juce::jlimit(0,3,voice);
    if(voice==3)return oklabRamp({kyraxStops[0],kyraxStops[5]},p);
    return from[voice].interpolatedWith(to[voice],p);
}
juce::Colour softColour(juce::Colour c,float amount) noexcept
{
    const auto linear=[](float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);};
    const auto srgb=[](float v){return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.0f/2.4f)-.055f;};
    float r=linear(c.getFloatRed()),g=linear(c.getFloatGreen()),b=linear(c.getFloatBlue());
    const float y=.2126f*r+.7152f*g+.0722f*b;
    return juce::Colour::fromFloatRGBA(srgb(y+amount*(r-y)),srgb(y+amount*(g-y)),srgb(y+amount*(b-y)),1);
}

juce::Colour approvedHoloColour(juce::Colour source, bool pinkEnd) noexcept
{
    const float redPart=source.getFloatRed(),green=source.getFloatGreen(),blue=source.getFloatBlue();
    const float mx=std::max({redPart,green,blue}),mn=std::min({redPart,green,blue});
    float hue=std::atan2(std::sqrt(3.0f)*(green-blue),2*redPart-green-blue)*180/juce::MathConstants<float>::pi;
    if(hue<0)hue+=360;
    if(mx<=0||(mx-mn)/mx<=.3f||hue<=120||hue>=235)return source;
    if(std::abs(hue-198)<8&&mx>.66f)return juce::Colour(150,96,255).withAlpha(source.getFloatAlpha());
    const auto position=std::floor(juce::jlimit(0.0f,1.0f,(198-hue)/45)*63)/63;
    const int stops[3][3]={{150,96,255},{196,100,201},{241,88,142}};
    const int panelStops[2][3]={{80,30,220},{255,60,160}};
    // pinkEnd (the small panel bars): violet to hot pink over the whole run, no yellow.
    const int index=pinkEnd||position<.5f?0:1;const auto fraction=pinkEnd?position:position<.5f?position*2:position*2-1;
    const auto brightness=juce::jlimit(0.0f,1.25f,mx/.58f);
    const auto channel=[&](int c){const int tableValue=pinkEnd?static_cast<int>(panelStops[0][c]+fraction*(panelStops[1][c]-panelStops[0][c])):static_cast<int>(stops[index][c]+fraction*(stops[index+1][c]-stops[index][c]));return static_cast<juce::uint8>(juce::jlimit(0,255,static_cast<int>(tableValue*brightness)));};
    return juce::Colour(channel(0),channel(1),channel(2)).withAlpha(source.getFloatAlpha());
}

const Fonts& Fonts::get()
{
    static const Fonts fonts { loadTypeface("Silkscreen-Regular.ttf"), loadTypeface("Silkscreen-Bold.ttf"),
                               loadTypeface("PressStart2P-Regular.ttf"), loadTypeface("Roboto-Regular.ttf") };
    return fonts;
}

// Silkscreen is drawn on an 8 px grid (one font pixel = 1/8 em). Small bold text is set at exactly 8 px, letters 5 px
// tall, so at 100 % every pixel of a letter is solid instead of smeared across two; drawText puts it on whole pixels.
// Regular text keeps its softer look, but is never set below 10 so the small lines still read.
juce::Font pixelFont(float height, bool bold)
{
    const auto options = juce::FontOptions(bold ? Fonts::get().pixelBold : Fonts::get().pixel);
    if (! bold) return juce::Font(options.withHeight(juce::jmax(10.0f, height)));     // regular text stays soft, never smaller than 10
    return juce::Font(height < 13.0f ? options.withPointHeight(crispPixelPoints) : options.withHeight(height));
}

juce::Font logoFont(float height)
{
    return juce::Font(juce::FontOptions(Fonts::get().logo).withHeight(height));
}

void bevel(juce::Graphics& g, juce::Rectangle<float> r, bool raised)
{
    using namespace win98;
    g.setColour(raised ? light : shadow);
    g.fillRect(r.getX(), r.getY(), r.getWidth(), 1.5f);
    g.fillRect(r.getX(), r.getY(), 1.5f, r.getHeight());
    g.setColour(raised ? dark : light);
    g.fillRect(r.getX(), r.getBottom() - 1.5f, r.getWidth(), 1.5f);
    g.fillRect(r.getRight() - 1.5f, r.getY(), 1.5f, r.getHeight());
    g.setColour(raised ? shadow : dark);
    if (raised)
    {
        g.fillRect(r.getX() + 1.5f, r.getBottom() - 3.0f, r.getWidth() - 3.0f, 1.5f);
        g.fillRect(r.getRight() - 3.0f, r.getY() + 1.5f, 1.5f, r.getHeight() - 3.0f);
    }
    else
    {
        g.fillRect(r.getX() + 1.5f, r.getY() + 1.5f, r.getWidth() - 3.0f, 1.5f);
        g.fillRect(r.getX() + 1.5f, r.getY() + 1.5f, 1.5f, r.getHeight() - 3.0f);
    }
}

// Buttons: the same frame width on every side, so nothing on a button moves when it is pressed. Light comes from
// the top left: raised is white then light grey on the top and left, black then grey on the bottom and right;
// pressed swaps them, black then grey on the top and left, and the button sinks in.
void button98(juce::Graphics& g, juce::Rectangle<float> r, bool pressed)
{
    using namespace win98;
    const juce::Colour lightGrey { 0xffdfdfdf };
    g.setColour(pressed ? juce::Colour(0xffb4b4b4) : face);
    g.fillRect(r);
    const auto ring = [&](float inset, juce::Colour topLeft, juce::Colour bottomRight)
    {
        const auto b = r.reduced(inset);
        g.setColour(topLeft);
        g.fillRect(b.getX(), b.getY(), b.getWidth(), 1.5f);
        g.fillRect(b.getX(), b.getY(), 1.5f, b.getHeight());
        g.setColour(bottomRight);
        g.fillRect(b.getX(), b.getBottom() - 1.5f, b.getWidth(), 1.5f);
        g.fillRect(b.getRight() - 1.5f, b.getY(), 1.5f, b.getHeight());
    };
    if (pressed) { ring(0.0f, dark, light); ring(1.5f, shadow, lightGrey); }
    else         { ring(0.0f, light, dark); ring(1.5f, lightGrey, shadow); }
}

void sunken(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill)
{
    g.setColour(fill);
    g.fillRect(r);
    bevel(g, r, false);
}

juce::Rectangle<float> windowClient(juce::Rectangle<float> r, float titleHeight) noexcept
{
    return { r.getX() + 6.0f, r.getY() + 8.0f + titleHeight, r.getWidth() - 12.0f, r.getHeight() - 14.0f - titleHeight };
}

juce::Rectangle<float> windowTitleBar(juce::Rectangle<float> r, float titleHeight) noexcept
{
    return { r.getX() + 4.0f, r.getY() + 4.0f, r.getWidth() - 8.0f, titleHeight };
}

juce::Rectangle<float> window98(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
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
        juce::ColourGradient holo(approvedHoloColour(sourceFrom,!active),bar.getX(),0,
                                 approvedHoloColour(sourceTo,!active),bar.getRight(),0,false);
        for(int stop=1;stop<128;++stop)
        {
            const auto t=static_cast<float>(stop)/128;
            holo.addColour(t,approvedHoloColour(sourceFrom.interpolatedWith(sourceTo,t),!active));
        }
        if(active)     // the top title bar: the wordmark's colours, cream from 85%
        {
            holo=juce::ColourGradient(kyraxTitle(0),bar.getX(),0,kyraxTitle(1),bar.getRight(),0,false);
            for(int stop=1;stop<64;++stop)holo.addColour(stop/64.0,kyraxTitle(stop/64.0f));
        }
        g.setGradientFill(holo);
    }
    // A single rectangle stays opaque across fractional DPI/zoom transforms.
    g.fillRect(bar);
    drawText(g, title, bar.reduced(4.0f, 0.0f), pixelFont(titleHeight >= 20.0f ? 12.0f : 10.0f, true), juce::Colours::white);
    return windowClient(r, titleHeight);
}

juce::Colour titleBarColour(int character, float t) noexcept
{
    if (character == 3) return kyraxTitle(t);
    const auto& p = paletteFor(character);
    return p.accent.darker(0.35f).interpolatedWith(p.secondary.darker(0.15f), juce::jlimit(0.0f, 1.0f, t));
}

juce::Colour valueInkFor(int character) noexcept
{
    return character == 3 ? approvedHoloColour(paletteFor(2).accent.darker(.6f)) : paletteFor(character).accent.darker(.6f);
}

void statusBar(juce::Graphics& g, juce::Rectangle<float> r, const juce::StringArray& fields, juce::Colour lastColour)
{
    float x = r.getX();
    const float each = r.getWidth() / static_cast<float>(juce::jmax(1, fields.size()));
    for (const auto& field : fields)
    {
        const juce::Rectangle<float> cell { x, r.getY(), each - 3.0f, r.getHeight() };
        bevel(g, cell, false);
        drawText(g, field, cell.reduced(5.0f, 0.0f), pixelFont(crispPixelPoints), &field == &fields.getReference(fields.size() - 1) ? lastColour : juce::Colours::black);
        x += each;
    }
}

void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Font& font,
              juce::Colour colour, juce::Justification justification)
{
    g.setColour(colour);
    g.setFont(font);
    if (std::abs(font.getHeightInPoints() - crispPixelPoints) > 0.01f) { g.drawText(text, r, justification, false); return; }
    // The 8 px pixel font: start and baseline on whole pixels, the 5 px tall letters centred in the box.
    const float w = juce::GlyphArrangement::getStringWidth(font, text);
    const float x = justification.testFlags(juce::Justification::horizontallyCentred) ? r.getCentreX() - w * 0.5f
                  : justification.testFlags(juce::Justification::right) ? r.getRight() - w : r.getX();
    const float baseline = justification.testFlags(juce::Justification::top) ? r.getY() + 5.0f
                         : justification.testFlags(juce::Justification::bottom) ? r.getBottom() - 1.0f : r.getCentreY() + 2.5f;
    g.drawSingleLineText(text, juce::roundToInt(x), juce::roundToInt(baseline));
}

juce::Path modeGlyph(int mode, juce::Rectangle<float> r)
{
    juce::Path p;
    const auto c = r.getCentre();
    const float s = r.getWidth() * 0.5f;
    switch (mode)
    {
        case 0:
            for (int k = 0; k < 3; ++k)
                p.addCentredArc(c.x - s * 0.5f, c.y, s * (0.45f + 0.35f * k), s * (0.45f + 0.35f * k), 0.0f,
                                juce::MathConstants<float>::pi * 0.30f, juce::MathConstants<float>::pi * 0.70f, true);
            break;
        case 1:     // two rings, sized to the shorter side so the outer one stays round, not clipped
        {
            const float d = juce::jmin(r.getWidth(), r.getHeight()) * 0.82f;
            p.addEllipse(r.withSizeKeepingCentre(d, d));
            p.addEllipse(r.withSizeKeepingCentre(d * 0.45f, d * 0.45f));
            break;
        }
        case 2:
            p.startNewSubPath(c.x - s * 0.9f, c.y + s * 0.5f);
            p.lineTo(c.x - s * 0.35f, c.y + s * 0.5f);
            p.lineTo(c.x - s * 0.1f, c.y - s * 0.8f);
            p.lineTo(c.x + s * 0.2f, c.y + s * 0.8f);
            p.lineTo(c.x + s * 0.45f, c.y - s * 0.2f);
            p.lineTo(c.x + s * 0.9f, c.y - s * 0.2f);
            break;
        case 3:
            p.startNewSubPath(c.x - s * 0.9f, c.y + s * 0.6f);
            p.cubicTo(c.x - s * 0.2f, c.y + s * 0.6f, c.x - s * 0.1f, c.y - s * 0.8f, c.x + s * 0.9f, c.y - s * 0.7f);
            break;
        default:
            p.startNewSubPath(c.x - s * 0.9f, c.y);
            for (int k = 1; k <= 8; ++k)
                p.lineTo(c.x - s * 0.9f + k * s * 0.225f, c.y + ((k % 2) != 0 ? -s * 0.22f : s * 0.22f));
            break;
    }
    return p;
}

float hash(int x, int y) noexcept
{
    uint32_t n = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return static_cast<float>(n & 0xffff) / 65535.0f;
}

// ------------------------------------------------------------------------------- WIN98 LOOK AND FEEL
Win98LookAndFeel::Win98LookAndFeel()
{
    setColour(juce::PopupMenu::backgroundColourId, win98::face);
    setColour(juce::PopupMenu::textColourId, juce::Colours::black);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xffffffe1));
    setColour(juce::TooltipWindow::textColourId, juce::Colours::black);
    setColour(juce::TooltipWindow::outlineColourId, juce::Colours::black);
}

juce::Font Win98LookAndFeel::getPopupMenuFont() { return pixelFont(12.0f); }

void Win98LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                                        juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = std::round(y + height * 0.5f);
    sunken(g, { static_cast<float>(x), cy - 2.0f, static_cast<float>(width), 4.0f }, win98::light);
    button98(g, { std::round(sliderPos) - 4.0f, cy - 8.0f, 9.0f, 16.0f }, false);
}

void Win98LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox&)
{
    const juce::Rectangle<float> r { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
    sunken(g, r, win98::light);
    const auto arrow = r.reduced(2.0f).withLeft(r.getRight() - 2.0f - (r.getHeight() - 4.0f));
    button98(g, arrow, down);
    const auto c = arrow.getCentre();
    g.setColour(juce::Colours::black);
    for (int i = 0; i < 3; ++i) g.fillRect(std::round(c.x) - 3.0f + i, std::round(c.y) - 1.0f + i, 7.0f - 2.0f * i, 1.0f);
}

juce::Font Win98LookAndFeel::getComboBoxFont(juce::ComboBox&) { return pixelFont(11.0f, true); }

void Win98LookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));
    if (label.isBeingEdited()) return;
    const auto ink = label.findColour(juce::Label::textColourId).withMultipliedAlpha(label.isEnabled() ? 1.0f : 0.5f);
    drawText(g, label.getText(), label.getBorderSize().subtractedFrom(label.getLocalBounds()).toFloat(), getLabelFont(label), ink,
             label.getJustificationType());
}

void Win98LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(3, 1, box.getWidth() - box.getHeight() - 3, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setColour(juce::Label::textColourId, juce::Colours::black);
}

void Win98LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool, bool)
{
    const auto r = button.getLocalBounds().toFloat();
    const auto box = juce::Rectangle<float>(13.0f, 13.0f).withPosition(r.getX(), std::round(r.getCentreY() - 6.5f));
    sunken(g, box, win98::light);
    if (button.getToggleState())
    {
        g.setColour(juce::Colours::black);
        const float pts[][2] { { -3, 0 }, { -2, 1 }, { -1, 2 }, { 0, 1 }, { 1, 0 }, { 2, -1 }, { 3, -2 } };
        for (const auto& p : pts) g.fillRect(box.getCentreX() + p[0] - 0.5f, box.getCentreY() + p[1] - 1.5f, 1.0f, 3.0f);
    }
    drawText(g, button.getButtonText(), r.withTrimmedLeft(18.0f), pixelFont(10.0f, true), juce::Colours::black);
}

void Win98LookAndFeel::drawPopupMenuBackgroundWithOptions(juce::Graphics& g, int width, int height, const juce::PopupMenu::Options&)
{
    const juce::Rectangle<float> r { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
    g.fillAll(win98::face);
    // Win98 raised frame: light grey / white on the top-left, black / grey on the bottom-right.
    g.setColour(juce::Colour(0xffdfdfdf)); g.fillRect(r.withHeight(1.0f)); g.fillRect(r.withWidth(1.0f));
    g.setColour(win98::light); g.fillRect(r.reduced(1.0f).withHeight(1.0f)); g.fillRect(r.reduced(1.0f).withWidth(1.0f));
    g.setColour(juce::Colours::black); g.fillRect(r.withTop(r.getBottom() - 1.0f)); g.fillRect(r.withLeft(r.getRight() - 1.0f));
    g.setColour(win98::shadow);
    g.fillRect(r.reduced(1.0f).withTop(r.getBottom() - 2.0f)); g.fillRect(r.reduced(1.0f).withLeft(r.getRight() - 2.0f));
}

void Win98LookAndFeel::drawPopupMenuItemWithOptions(juce::Graphics& g, const juce::Rectangle<int>& area, bool isHighlighted,
                                                    const juce::PopupMenu::Item& item, const juce::PopupMenu::Options&)
{
    auto r = area.toFloat();
    if (item.isSeparator)
    {
        const float y = std::round(r.getCentreY());
        g.setColour(win98::shadow); g.fillRect(r.getX() + 2.0f, y - 1.0f, r.getWidth() - 4.0f, 1.0f);
        g.setColour(win98::light); g.fillRect(r.getX() + 2.0f, y, r.getWidth() - 4.0f, 1.0f);
        return;
    }
    const bool hot = isHighlighted && item.isEnabled;
    if (hot)
    {
        g.setColour(paletteFor(character).accent.darker(0.35f));     // the title-bar colour
        g.fillRect(r.reduced(1.0f, 0.0f));
    }
    const auto ink = hot ? juce::Colours::white : juce::Colours::black;
    const auto textArea = r.withTrimmedLeft(22.0f).withTrimmedRight(10.0f);
    const auto font = getPopupMenuFont();
    if (! item.isEnabled)        // Win98 disabled text: grey with a white emboss
        drawText(g, item.text, textArea.translated(1.0f, 1.0f), font, win98::light);
    drawText(g, item.text, textArea, font, item.isEnabled ? ink : win98::shadow);
    if (item.shortcutKeyDescription.isNotEmpty())
        drawText(g, item.shortcutKeyDescription, textArea, pixelFont(8.0f), hot ? juce::Colour(0xffdfdfdf) : win98::shadow,
                 juce::Justification::centredRight);

    if (item.isTicked)
    {
        const float cx = r.getX() + 11.0f, cy = std::round(r.getCentreY());
        g.setColour(item.isEnabled ? ink : win98::shadow);
        if (item.itemID >= radioIdFirst && item.itemID <= radioIdLast)
        {
            g.fillRect(cx - 1.0f, cy - 3.0f, 3.0f, 6.0f);           // pixel radio dot
            g.fillRect(cx - 2.0f, cy - 2.0f, 5.0f, 4.0f);
        }
        else
        {
            const float pts[][2] { { -3, 0 }, { -2, 1 }, { -1, 2 }, { 0, 1 }, { 1, 0 }, { 2, -1 }, { 3, -2 } };
            for (const auto& p : pts) g.fillRect(cx + p[0], cy + p[1] - 1.0f, 1.0f, 3.0f);     // pixel checkmark
        }
    }
}

void Win98LookAndFeel::getIdealPopupMenuItemSizeWithOptions(const juce::String& text, bool isSeparator, int, int& idealWidth,
                                                            int& idealHeight, const juce::PopupMenu::Options&)
{
    idealHeight = isSeparator ? 8 : 20;
    idealWidth = juce::roundToInt(juce::GlyphArrangement::getStringWidth(getPopupMenuFont(), text)) + 70;
}

juce::Rectangle<int> Win98LookAndFeel::getTooltipBounds(const juce::String& text, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(pixelFont(12.0f), text)) + 14, h = 20;
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin(parentArea);
}

void Win98LookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const juce::Rectangle<float> r { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
    g.fillAll(juce::Colour(0xffffffe1));
    g.setColour(juce::Colours::black);
    g.drawRect(r, 1.0f);
    drawText(g, text, r.reduced(7.0f, 0.0f), pixelFont(12.0f), juce::Colours::black);
}

void Win98LookAndFeel::drawCornerResizer(juce::Graphics& g, int width, int height, bool, bool)
{
    // Win98 size grip: three diagonal ridges, each a white line over a grey one.
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    for (int ridge = 0; ridge < 3; ++ridge)
    {
        const float off = 3.0f + static_cast<float>(ridge) * 4.0f;
        for (float t = 0.0f; t < w - off; t += 1.0f)
        {
            g.setColour(win98::light);  g.fillRect(off + t, h - 1.0f - t, 1.0f, 1.0f);
            g.setColour(win98::shadow); g.fillRect(off + t + 1.0f, h - 1.0f - t, 1.0f, 1.0f);
            g.fillRect(off + t + 2.0f, h - 1.0f - t, 1.0f, 1.0f);
        }
    }
}
}
