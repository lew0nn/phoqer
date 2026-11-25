// The Win98 controls: mode buttons, the menu bar, arrows, the preset box, REC, title-bar buttons,
// push buttons, number fields, the metronome button and the LCD readouts.

#include "Views.h"

#include "../Core/FishSprite.h"

#include "Style.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace phoqer::ui
{
// -------------------------------------------------------------------------------------- BUTTONS
ChoiceButton::ChoiceButton(Kind k, int i, const juce::String& label, juce::RangedAudioParameter& parameter)
    : juce::Button(label), kind(k), index(i),
      attachment(parameter, [this](float value) { selected = juce::roundToInt(value) == index; repaint(); }, nullptr)
{
    setWantsKeyboardFocus(false);
    setTooltip(label);
    onClick = [this]
    {
        if (onReselect != nullptr && (selected || popupClick)) { onReselect(); return; }
        attachment.setValueAsCompleteGesture(static_cast<float>(index));
    };
    attachment.sendInitialUpdate();
}

void ChoiceButton::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto r = getLocalBounds().toFloat();
    const bool pressed = selected || down;
    button98(g, r, pressed);
    const float nudge = 0.0f;     // nothing moves when pressed: the frame does the pressing
    if (kind == Kind::voiceTab)
    {
        if (selected)
        {
            // Each tab shows its own voice colour when selected.
            g.setColour(paletteFor(index).accent);
            g.fillRect(r.reduced(4.0f));
        }
        drawText(g, getButtonText(), r.translated(nudge, nudge), pixelFont(11.0f, true), juce::Colours::black, juce::Justification::centred);
        return;
    }
    // Mode button: keep the glyph black and readable while a narrow signal rail carries the
    // character colour.  Every rail is faintly alive; the selected mode drives it at full LED
    // brightness and stays physically depressed in the Win98 chassis.
    const auto& pal = paletteFor(character);
    const auto dimSignal = character==3?approvedHoloColour(paletteFor(2).accent.darker(1.75f).withMultipliedSaturation(.72f)):pal.accent.darker(1.75f).withMultipliedSaturation(.72f);
    const auto signal = selected ? pal.accent : dimSignal;
    const auto rail = juce::Rectangle<float> { r.getX() + 5.0f + nudge,
                                               r.getY() + 7.0f + nudge,
                                               3.0f,
                                               r.getHeight() - 14.0f };
    if (selected)
    {
        g.setColour(pal.accent.withAlpha(0.18f));
        g.fillRect(rail.expanded(1.0f, 1.0f));
    }
    g.setColour(signal);
    g.fillRect(rail);

    // The glyph remains a 36 x 24 pixel image, shown 1:1 without smoothing: still pixelated,
    // half the softness of the old 18 x 12 one enlarged.
    constexpr int w = 36, h = 24;
    juce::Image icon(juce::Image::ARGB, w, h, true);
    {
        juce::Graphics ig(icon);
        ig.addTransform(juce::AffineTransform::scale(static_cast<float>(w) / 40.0f, static_cast<float>(h) / 26.0f));
        ig.setColour(juce::Colours::black);
        // centred by what it actually draws: some glyphs (CALL's waves) sit off their layout box
        auto glyph = modeGlyph(index, { 4.0f, 2.0f, 32.0f, 22.0f });
        juce::Path outline;
        juce::PathStrokeType(4.5f).createStrokedPath(outline, glyph);
        const auto drawn = outline.getBounds();
        glyph.applyTransform(juce::AffineTransform::translation(20.0f - drawn.getCentreX(), 13.0f - drawn.getCentreY()));
        ig.strokePath(glyph, juce::PathStrokeType(4.5f));
    }
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    const auto area = juce::Rectangle<float>(36.0f, 24.0f).withCentre(r.getCentre().translated(nudge, -6.0f));
    g.drawImage(icon, area.withPosition(std::round(area.getX()), std::round(area.getY())));
    drawText(g, getButtonText(), r.withTrimmedTop(r.getHeight() - 15.0f).translated(nudge, -1.0f), pixelFont(9.0f),
             juce::Colours::black, juce::Justification::centred);
}

// ------------------------------------------------------------------------------------- MENU BAR
juce::Rectangle<float> MenuBar98::itemBounds(int index) const
{
    float x = 2.0f;
    for (int i = 0; i < itemCount; ++i)
    {
        const float w = std::round(juce::GlyphArrangement::getStringWidth(pixelFont(12.0f), items[i])) + 14.0f;
        if (i == index) return { x, 0.0f, w, static_cast<float>(getHeight() - 4) };
        x += w + 2.0f;
    }
    return {};
}

void MenuBar98::setOpenIndex(int index)
{
    openIndex = index;
    repaint();
}

void MenuBar98::paint(juce::Graphics& g)
{
    for (int i = 0; i < itemCount; ++i)
    {
        const auto r = itemBounds(i);
        const bool open = i == openIndex;
        if (open) bevel(g, r, false);                              // Win98: the open menu's title is pressed in
        const auto text = r.translated(open ? 1.0f : 0.0f, open ? 1.0f : 0.0f).withTrimmedLeft(7.0f);
        drawText(g, items[i], text, pixelFont(12.0f), juce::Colours::black);
        // Mnemonic underline, exactly under the first letter.
        const float first = std::round(juce::GlyphArrangement::getStringWidth(pixelFont(12.0f), juce::String::charToString(items[i][0])));
        g.setColour(juce::Colours::black);
        g.fillRect(text.getX(), std::round(text.getCentreY() + 5.0f), first - 1.0f, 1.0f);
    }
    g.setColour(win98::shadow);
    g.fillRect(0.0f, static_cast<float>(getHeight() - 2), static_cast<float>(getWidth()), 1.0f);
    g.setColour(win98::light);
    g.fillRect(0.0f, static_cast<float>(getHeight() - 1), static_cast<float>(getWidth()), 1.0f);
}

void MenuBar98::mouseDown(const juce::MouseEvent& e)
{
    for (int i = 0; i < itemCount; ++i)
        if (itemBounds(i).contains(e.position) && onOpen) { onOpen(i); return; }
}

// --------------------------------------------------------------------------------- ARROW BUTTON
ArrowButton98::ArrowButton98(bool pointsLeft) : juce::Button({}), left(pointsLeft)
{
    setWantsKeyboardFocus(false);
    setTooltip(pointsLeft ? "PREVIOUS PRESET" : "NEXT PRESET");
}

void ArrowButton98::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto b = getLocalBounds().toFloat();
    button98(g, b, down);
    const float cx = std::floor(b.getCentreX()) , cy = std::floor(b.getCentreY()) ;
    g.setColour(juce::Colours::black);
    for (int k = 0; k < 4; ++k)
        g.fillRect(left ? cx - 2.0f + static_cast<float>(k) : cx + 1.0f - static_cast<float>(k), cy - static_cast<float>(k), 1.0f, 1.0f + 2.0f * static_cast<float>(k));
}

// ----------------------------------------------------------------------------------- PRESET BOX
PresetBox98::PresetBox98()
{
    setWantsKeyboardFocus(false);
    previous.onClick = [this] { if (onPrevious) onPrevious(); };
    next.onClick = [this] { if (onNext) onNext(); };
    addAndMakeVisible(previous);
    addAndMakeVisible(next);
    setTooltip("PRESET  -  CLICK FOR THE LIST");
}

juce::Rectangle<float> PresetBox98::fieldBounds() const
{
    return getLocalBounds().toFloat().withTrimmedRight(38.0f);
}

void PresetBox98::resized()
{
    const auto r = getLocalBounds().toFloat();
    previous.setBounds(juce::Rectangle<float>(r.getRight() - 36.0f, r.getY(), 17.0f, r.getHeight()).toNearestInt());
    next.setBounds(juce::Rectangle<float>(r.getRight() - 17.0f, r.getY(), 17.0f, r.getHeight()).toNearestInt());
}

void PresetBox98::paint(juce::Graphics& g)
{
    const auto f = fieldBounds();
    sunken(g, f);
    drawText(g, text ? text() : juce::String(), f.reduced(7.0f, 0.0f), pixelFont(12.0f), juce::Colours::black);
}

void PresetBox98::mouseDown(const juce::MouseEvent& e)
{
    if (fieldBounds().contains(e.position) && onOpenList) onOpenList();
}

// ------------------------------------------------------------------------------- RECORD CONTROL
RecordControl98::RecordControl98()
{
    setWantsKeyboardFocus(false);
    setTooltip("RECORD TO WAV");
}

juce::Rectangle<float> RecordControl98::buttonBounds() const
{
    return getLocalBounds().toFloat().withWidth(52.0f);
}

void RecordControl98::setState(bool isRecording, double seconds, int countInBeats)
{
    const int s = static_cast<int>(seconds);
    if (isRecording == recording && s == shownSeconds && countInBeats == shownCount) return;
    recording = isRecording;
    shownSeconds = s;
    shownCount = countInBeats;
    repaint();
}

void RecordControl98::paint(juce::Graphics& g)
{
    const auto b = buttonBounds();
    button98(g, b, recording);                                         // stays pressed in while recording
    const float o = 0.0f;     // REC stays put while pressed in
    g.setColour(recording ? juce::Colour(0xffe61428) : juce::Colour(0xff783240));
    g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({ b.getX() + 10.0f + o, b.getCentreY() + o }));
    drawText(g, "REC", b.withTrimmedLeft(17.0f).translated(o, o), pixelFont(12.0f), juce::Colours::black);
    const auto lcd = getLocalBounds().toFloat().withTrimmedLeft(56.0f);
    sunken(g, lcd, juce::Colour(0xff10080c));
    const auto t = shownCount > 0 ? "- " + juce::String(shownCount) + " -"
                                  : juce::String(shownSeconds / 60).paddedLeft('0', 2) + ":" + juce::String(shownSeconds % 60).paddedLeft('0', 2);
    // The 7 px digits centred in the dark of the LCD (under its 3-unit top edge, above its 1.5-unit bottom one), on whole pixels.
    const auto font = logoFont(8.0f);
    const auto inside = lcd.withTrimmedTop(3.0f).withTrimmedBottom(1.5f);
    g.setColour(recording ? juce::Colour(0xffff5064) : juce::Colour(0xff6e3c46));
    g.setFont(font);
    g.drawSingleLineText(t, juce::roundToInt(inside.getCentreX() - juce::GlyphArrangement::getStringWidth(font, t) * 0.5f),
                         juce::roundToInt(inside.getCentreY() + 3.5f));
}

void RecordControl98::mouseDown(const juce::MouseEvent& e)
{
    if (buttonBounds().contains(e.position) && onToggle) onToggle();
}

// ---------------------------------------------------------------------------------- TITLE BUTTONS
TitleButton98::TitleButton98(Kind k) : juce::Button({}), kind(k)
{
    setWantsKeyboardFocus(false);
    setTooltip(k == Kind::minimise ? "MINIMIZE" : k == Kind::maximise ? "FULL SCREEN" : "CLOSE");
}

void TitleButton98::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto b = getLocalBounds().toFloat();
    button98(g, b, down);
    // Pixel glyphs as Win98 drew them; they stay put while pressed (the frame does the pressing).
    const float cx = std::floor(b.getCentreX()) , cy = std::floor(b.getCentreY()) ;
    g.setColour(juce::Colours::black);
    if (kind == Kind::close)
    {
        for (int i = 0; i < 8; ++i)
        {
            g.fillRect(cx - 4.0f + i, cy - 4.0f + i, 2.0f, 1.0f);
            g.fillRect(cx + 2.0f - i, cy - 4.0f + i, 2.0f, 1.0f);
        }
    }
    else if (kind == Kind::maximise)
    {
        const juce::Rectangle<float> box { cx - 5.0f, cy - 5.0f, 10.0f, 9.0f };
        g.fillRect(box.withHeight(2.0f));
        g.fillRect(box.withTop(box.getBottom() - 1.0f));
        g.fillRect(box.withWidth(1.0f));
        g.fillRect(box.withLeft(box.getRight() - 1.0f));
    }
    else
        g.fillRect(cx - 4.0f, cy + 2.0f, 7.0f, 2.0f);
}

// ---------------------------------------------------------------------------------- PUSH BUTTON
PushButton98::PushButton98(const juce::String& label, bool isDefaultButton) : juce::Button(label), isDefault(isDefaultButton)
{
    setWantsKeyboardFocus(false);
}

void PushButton98::paintButton(juce::Graphics& g, bool, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (isDefault)
    {
        g.setColour(juce::Colours::black);
        g.drawRect(r, 1.0f);
        r = r.reduced(1.0f);
    }
    button98(g, r, down);
    drawText(g, getButtonText(), r, pixelFont(12.0f, true), juce::Colours::black,
             juce::Justification::centred);
}

NumberField98::NumberField98(juce::RangedAudioParameter& parameter, const juce::String& labelText, int lowest,
                             int highest, float size)
    : label(labelText), minimum(lowest), maximum(highest), textSize(size),
      attachment(parameter, [this](float v) { show(v); }, nullptr)
{
    setTooltip(label + ": CLICK TO TYPE " + juce::String(minimum) + "-" + juce::String(maximum));
    field.setName(label);
    field.setBorderSize(juce::BorderSize<int>(0));
    field.setJustificationType(juce::Justification::centred);
    field.setEditable(true, true, false);
    setCharacter(0);
    field.onEditorShow = [this]
    {
        if (onBeginEdit) onBeginEdit();
        if (auto* editor = field.getCurrentTextEditor())
        {
            editor->setInputRestrictions(juce::String(maximum).length(), "0123456789");
            editor->setJustification(juce::Justification::centred);
            editor->setText(juce::String(juce::roundToInt(value)), false);
            editor->selectAll();
        }
    };
    field.onTextChange = [this]
    {
        const auto text = field.getText().trim();
        const auto next = text.isEmpty() ? value : static_cast<float>(juce::jlimit(minimum, maximum, text.getIntValue()));
        attachment.setValueAsCompleteGesture(next);
        show(next);
    };
    addAndMakeVisible(field);
    attachment.sendInitialUpdate();
}

void NumberField98::show(float v)
{
    value = v;
    field.setText(juce::String(juce::roundToInt(v)), juce::dontSendNotification);
}

void NumberField98::setCharacter(int c)
{
    character = c;
    const auto& pal = paletteFor(character);
    field.setFont(pixelFont(textSize, true));
    for (const auto id : { juce::Label::textColourId, juce::Label::textWhenEditingColourId })
        field.setColour(id, pal.accent);
    for (const auto id : { juce::Label::backgroundColourId, juce::Label::backgroundWhenEditingColourId })
        field.setColour(id, pal.sky);
    repaint();
}

juce::Rectangle<int> NumberField98::fieldArea() const
{
    return getLocalBounds().withTrimmedLeft(28);
}

void NumberField98::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    drawText(g, label, r.withWidth(26.0f), pixelFont(10.0f, true), juce::Colours::black, juce::Justification::centredRight);
    sunken(g, fieldArea().toFloat(), paletteFor(character).sky);
}

void NumberField98::resized() { field.setBounds(fieldArea().reduced(2)); }

MetronomeButton98::MetronomeButton98() : juce::Button("METRONOME")
{
    setClickingTogglesState(true);
    setTooltip("METRONOME CLICK ON / OFF: FOLLOWS BPM; NOT INCLUDED IN WAV TAKES");
}

void MetronomeButton98::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    button98(g, r, on || down);
    const auto& pal = paletteFor(character);
    const auto glassArea = r.reduced(r.getHeight() < 24.0f ? 2.0f : 3.0f);     // small (header) size: room for the 16-unit icon
    g.setColour(pal.sky);
    g.fillRect(glassArea);

    // n x n pixels (16, or fewer in the status bar): the wireframe body in the accent, the arm and its
    // weight in the second colour, a dim halo round every lit pixel for the neon bleed. Drawn on a
    // 16-pixel design and scaled to whole pixels, so the small one stays crisp.
    const int n = juce::jlimit(9, 16, static_cast<int>(glassArea.getHeight()));
    const float k = (n - 1) / 15.0f;
    const auto at = [k](float v) { return juce::roundToInt(v * k); };
    juce::Image icon(juce::Image::ARGB, n, n, true);
    const auto plot = [&](int x, int y, juce::Colour c) { if (x >= 0 && y >= 0 && x < n && y < n) icon.setPixelAt(x, y, c); };
    const auto line = [&](int x0, int y0, int x1, int y1, juce::Colour c)
    {
        const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (;;)
        {
            plot(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    };
    const juce::Colour dim[]={juce::Colour(120,18,40),juce::Colour(96,24,120),juce::Colour(16,70,98),juce::Colour(92,44,184)};
    const juce::Colour edge[]={juce::Colour(255,45,85),juce::Colour(224,75,255),juce::Colour(35,150,200),juce::Colour(150,96,255)};
    const juce::Colour arm[]={juce::Colour(255,160,60),juce::Colour(90,210,240),juce::Colour(80,214,140),juce::Colour(240,100,210)};
    const int theme=juce::jlimit(0,3,character);
    line(at(5),at(1),at(10),at(1),dim[theme]);line(at(10),at(1),at(14),at(14),dim[theme]);line(at(14),at(14),at(1),at(14),dim[theme]);line(at(1),at(14),at(5),at(1),dim[theme]);
    line(at(6),at(2),at(9),at(2),edge[theme]);line(at(9),at(2),at(13),at(13),edge[theme]);line(at(13),at(13),at(2),at(13),edge[theme]);line(at(2),at(13),at(6),at(2),edge[theme]);
    const float b=on&&beats?beats():0.0f;
    const float angle=on ? juce::degreesToRadians(34.0f)*std::cos(juce::MathConstants<float>::pi*b) : -std::atan2(3.0f,9.0f);
    const float length=std::sqrt(90.0f);
    const int tipX=at(8+length*std::sin(angle)),tipY=at(12-length*std::cos(angle));
    line(at(8),at(12),tipX,tipY,arm[theme]);plot(tipX,tipY,juce::Colours::white);
    const float sinceClick=on&&beats?beats()-std::floor(beats()):1.0f;
    const float flash=juce::square(juce::jmax(0.0f,1.0f-sinceClick/.3f));
    juce::Image glowing(juce::Image::ARGB, n, n, true);
    const auto halo = pal.accent.withMultipliedBrightness(0.42f);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            if (icon.getPixelAt(x, y).getAlpha() > 0) { glowing.setPixelAt(x, y, icon.getPixelAt(x, y)); continue; }
            for (const auto& [dx, dy] : { std::pair { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } })
                if (x + dx >= 0 && y + dy >= 0 && x + dx < n && y + dy < n && icon.getPixelAt(x + dx, y + dy).getAlpha() > 0)
                    { glowing.setPixelAt(x, y, halo); break; }
        }
    if (flash > 0.0f)
    {
        g.setColour(pal.secondary.withAlpha(0.35f * flash));
        g.drawRect(glassArea, 1.0f);
    }
    g.setOpacity(on || down ? 1.0f : 0.7f);     // off: dimmed. (drawImage takes the current colour's alpha, so set it.)
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    // Whole or half units per icon pixel, so it stays crisp at 2x.
    const float scale = juce::jmax(1.0f, std::floor(glassArea.getHeight() / static_cast<float>(n) * 2.0f) * 0.5f);
    g.drawImage(glowing, juce::Rectangle<float>(n * scale, n * scale).withCentre(glassArea.getCentre()));
}

DiceButton98::DiceButton98() : juce::Button("RANDOM")
{
    setTooltip("RANDOM SOUND: SHUFFLES THE KNOBS (EDIT > RANDOMIZE KNOBS)");
}

void DiceButton98::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto r = getLocalBounds().toFloat();
    button98(g, r, down);
    const auto& pal = paletteFor(character);
    const auto glassArea = r.reduced(r.getHeight() < 24.0f ? 2.0f : 3.0f);
    g.setColour(pal.sky);
    g.fillRect(glassArea);
    // 14 x 14 pixels in a 16 x 16 icon (a pixel of halo all round). O edge, t/l/r the three faces, p a pip.
    static const char* const cube[] { "      OO      ", "    OOttOO    ", "  OOttttttOO  ", "OOttttppttttOO", "OlOOttttttOOrO",
                                      "OlllOOttOOrrrO", "OlplllOOrrrprO", "OlllllOOrrrrrO", "OlllllOOrrprrO", "OlllllOOrrrrrO",
                                      "OOllplOOrprrOO", "  OOllOOrrOO  ", "    OOOOOO    ", "      OO      " };
    const juce::Colour edges[] { juce::Colour(255, 45, 85), juce::Colour(224, 75, 255), juce::Colour(35, 150, 200), juce::Colour(150, 96, 255) };
    const juce::Colour pips[] { juce::Colour(255, 160, 60), juce::Colour(90, 210, 240), juce::Colour(80, 214, 140), juce::Colour(240, 100, 210) };
    const int theme = juce::jlimit(0, 3, character);
    const auto edge = edges[theme], pip = pips[theme];
    const auto top = pal.sky.interpolatedWith(edge, 0.3f), left = pal.sky.interpolatedWith(edge, 0.13f), right = pal.sky;
    const auto halo = pal.accent.withMultipliedBrightness(0.42f);
    juce::Image icon(juce::Image::ARGB, 16, 16, true);
    for (int y = 0; y < 14; ++y)
        for (int x = 0; x < 14; ++x)
        {
            const char ch = cube[y][x];
            if (ch == ' ') continue;
            icon.setPixelAt(x + 1, y + 1, ch == 'O' ? edge : ch == 't' ? top : ch == 'l' ? left : ch == 'r' ? right : pip);
        }
    for (int y = 0; y < 16; ++y)     // the neon bleed: every empty pixel touching the edge
        for (int x = 0; x < 16; ++x)
        {
            if (icon.getPixelAt(x, y).getAlpha() > 0) continue;
            for (const auto& [dx, dy] : { std::pair { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } })
                if (x + dx >= 0 && y + dy >= 0 && x + dx < 16 && y + dy < 16 && icon.getPixelAt(x + dx, y + dy) == edge)
                    { icon.setPixelAt(x, y, halo); break; }
        }
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    const float scale = juce::jmax(1.0f, std::floor(glassArea.getHeight() / 16.0f * 2.0f) * 0.5f);
    g.drawImage(icon, juce::Rectangle<float>(16.0f * scale, 16.0f * scale).withCentre(glassArea.getCentre()));     // nothing on a button moves when it is pressed
}

void Readout98::mouseDown(const juce::MouseEvent& e)
{
    dragSent = 0;
    dragged = false;
    juce::Button::mouseDown(e);
}

void Readout98::mouseDrag(const juce::MouseEvent& e)
{
    if (! onDrag) { juce::Button::mouseDrag(e); return; }
    const int steps = juce::roundToInt(-e.getDistanceFromDragStartY() / 3.0f);
    if (steps == dragSent) return;
    dragged = true;
    onDrag(steps - dragSent);
    dragSent = steps;
}

void Readout98::mouseUp(const juce::MouseEvent& e)
{
    if (! dragged) { juce::Button::mouseUp(e); return; }
    setState(juce::Button::buttonNormal);     // a drag, not a click: METRO.EXE stays as it was
}

void Readout98::paintButton(juce::Graphics& g, bool over, bool)
{
    const auto& pal = paletteFor(character);
    const auto r = getLocalBounds().toFloat();
    sunken(g, r, pal.sky);
    drawText(g, shown, r, pixelFont(getHeight() >= 22 ? 14.0f : getHeight() >= 18 ? 12.0f : 10.0f, true), over ? pal.accent.brighter(0.4f) : pal.accent, juce::Justification::centred);
}
}
