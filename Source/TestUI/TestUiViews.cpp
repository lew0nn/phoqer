#include "TestUiViews.h"

#include "TestUiStyle.h"

#include <cmath>

namespace phoqer::testui
{
// ---------------------------------------------------------------------------------------- SEAL
void SealView::update(const PortraitRenderer& renderer, int c, const Expression& ex)
{
    character = c;
    intensity = ex.face.intensity;
    bitmap = renderer.render(c, ex, juce::jmax(8, getWidth() / 2), juce::jmax(8, getHeight() / 2));
    repaint();
}

void SealView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    if (! bitmap.isValid()) return;
    const auto& pal = paletteFor(character);
    const auto area = getLocalBounds().toFloat();
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImage(bitmap, juce::Rectangle<float>(0.0f, 0.0f, bitmap.getWidth() * 2.0f, bitmap.getHeight() * 2.0f));
    // Glitch slices across the chest, driven by how hard the seal is calling.
    const std::array<std::pair<float, float>, 2> slices { { { 0.86f, 8.0f * intensity }, { 0.93f, -6.0f * intensity } } };
    for (size_t i = 0; i < slices.size(); ++i)
    {
        const float y = std::round(area.getHeight() * slices[i].first / 2.0f) * 2.0f;
        const int sourceRow = static_cast<int>(y / 2.0f);
        g.drawImage(bitmap, static_cast<int>(slices[i].second), static_cast<int>(y), getWidth(), 4, 0, sourceRow, bitmap.getWidth(), 2);
        g.setColour((i == 0 ? pal.accent : pal.secondary).withAlpha(0.35f));
        g.fillRect(slices[i].second, y, area.getWidth(), 4.0f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    for (float y = 0.0f; y < area.getHeight(); y += 2.0f) g.fillRect(0.0f, y, area.getWidth(), 1.0f);
}

// --------------------------------------------------------------------------------------- SCOPE
ScopeView::ScopeView(const TelemetryPublisher& source) : telemetry(source)
{
    lastWriteIndex = telemetry.getWaveformWriteIndex();
    lastCallSerial = telemetry.getCallSerial();
}

void ScopeView::refresh()
{
    // Same capture behaviour as the main editor's waveform display: a call is captured from its
    // first sample, compacted when it outgrows the display, held, then faded.
    const double rate = juce::jmax(8000.0f, telemetry.getSampleRate());
    const auto end = telemetry.copyWaveform(capture);
    const auto serial = telemetry.getCallSerial();
    const bool newCall = serial != lastCallSerial;
    lastCallSerial = serial;
    uint32_t available = juce::jmin(end - lastWriteIndex, TelemetryPublisher::waveformSize);
    const double delta = juce::jmin(0.1, available / rate);
    if (newCall || (captureState == CaptureState::idle && telemetry.getPeak() >= 0.0015f))
    {
        positive.fill(0.0f);
        negative.fill(0.0f);
        usedBuckets = 0;
        samplesInBucket = 0;
        samplesPerBucket = juce::jmax(1, juce::roundToInt(rate / 1000.0));
        available = juce::jmin(TelemetryPublisher::waveformSize, available + static_cast<uint32_t>(rate * 0.025));
        capturePeak = 0.02f;
        displayGain = 1.0f;
        traceOpacity = 1.0f;
        silentSeconds = holdSeconds = 0.0;
        captureState = CaptureState::capturing;
    }
    lastWriteIndex = end;
    if (captureState == CaptureState::capturing)
    {
        float recentPeak = 0.0f;
        for (size_t i = capture.size() - available; i < capture.size(); ++i)
        {
            if (usedBuckets == bucketCount)
            {
                for (int b = 0; b < bucketCount / 2; ++b)
                {
                    positive[static_cast<size_t>(b)] = juce::jmax(positive[static_cast<size_t>(b * 2)], positive[static_cast<size_t>(b * 2 + 1)]);
                    negative[static_cast<size_t>(b)] = juce::jmin(negative[static_cast<size_t>(b * 2)], negative[static_cast<size_t>(b * 2 + 1)]);
                }
                std::fill(positive.begin() + bucketCount / 2, positive.end(), 0.0f);
                std::fill(negative.begin() + bucketCount / 2, negative.end(), 0.0f);
                usedBuckets = bucketCount / 2;
                samplesPerBucket *= 2;
            }
            const float value = capture[i];
            positive[static_cast<size_t>(usedBuckets)] = juce::jmax(positive[static_cast<size_t>(usedBuckets)], value);
            negative[static_cast<size_t>(usedBuckets)] = juce::jmin(negative[static_cast<size_t>(usedBuckets)], value);
            recentPeak = juce::jmax(recentPeak, std::abs(value));
            if (++samplesInBucket >= samplesPerBucket) { ++usedBuckets; samplesInBucket = 0; }
        }
        capturePeak = juce::jmax(capturePeak, recentPeak);
        const float target = juce::jlimit(0.01f, 14.0f, 0.86f / capturePeak);
        displayGain = target < displayGain ? target : displayGain + (target - displayGain) * 0.18f;
        capturedMs = static_cast<float>(1000.0 * usedBuckets * samplesPerBucket / rate);
        silentSeconds = recentPeak < 0.0004f ? silentSeconds + delta : 0.0;
        if (silentSeconds >= 0.05) { captureState = CaptureState::holding; holdSeconds = 0.0; }
    }
    else if (captureState == CaptureState::holding)
    {
        holdSeconds += delta;
        if (holdSeconds >= 0.5) captureState = CaptureState::decaying;
    }
    else if (captureState == CaptureState::decaying)
    {
        traceOpacity *= static_cast<float>(std::exp(-delta * 4.0));
        if (traceOpacity < 0.015f) { captureState = CaptureState::idle; usedBuckets = 0; }
    }
    repaint();
}

void ScopeView::paint(juce::Graphics& g)
{
    const auto& pal = paletteFor(character);
    g.fillAll(juce::Colours::black);
    const auto plot = getLocalBounds().toFloat().reduced(8.0f, 8.0f);
    g.setColour(pal.accent.withAlpha(0.28f));
    for (int k = 0; k <= 10; ++k) g.fillRect(plot.getX() + k * plot.getWidth() / 10.0f, plot.getY(), 1.0f, plot.getHeight());
    for (int k = 0; k <= 6; ++k) g.fillRect(plot.getX(), plot.getY() + k * plot.getHeight() / 6.0f, plot.getWidth(), 1.0f);
    const int count = juce::jmin(bucketCount, usedBuckets + (samplesInBucket > 0 ? 1 : 0));
    if (count < 2 || traceOpacity <= 0.0f) return;
    // Columns three units apart; the call fills the full width.
    const int columns = static_cast<int>(plot.getWidth() / 3.0f);
    for (int col = 0; col < columns; ++col)
    {
        const int first = col * count / columns, last = juce::jmax(first + 1, (col + 1) * count / columns);
        float hi = 0.0f, lo = 0.0f;
        for (int b = first; b < last; ++b) { hi = juce::jmax(hi, positive[static_cast<size_t>(b)]); lo = juce::jmin(lo, negative[static_cast<size_t>(b)]); }
        const float top = plot.getCentreY() - juce::jmin(1.0f, hi * displayGain) * plot.getHeight() * 0.48f;
        const float bottom = plot.getCentreY() - juce::jmax(-1.0f, lo * displayGain) * plot.getHeight() * 0.48f;
        const float x = std::floor(plot.getX() + col * 3.0f);
        const float y = std::floor(top / 2.0f) * 2.0f, h = juce::jmax(2.0f, std::floor((bottom - top) / 2.0f) * 2.0f);
        g.setColour(pal.accent.withAlpha(0.18f * traceOpacity));          // soft glow
        g.fillRect(x - 2.0f, y - 2.0f, 6.0f, h + 4.0f);
        g.setColour(pal.accent.withAlpha(traceOpacity));
        g.fillRect(x, y, 2.0f, h);
    }
}

// --------------------------------------------------------------------------------------- METER
void MeterView::refresh()
{
    peak = juce::jmax(telemetry.getPeak(), peak * 0.88f);
    repaint();
}

void MeterView::paint(juce::Graphics& g)
{
    const auto& pal = paletteFor(character);
    const juce::Rectangle<float> bar { 0.0f, 0.0f, static_cast<float>(getWidth()), 24.0f };
    sunken(g, bar, juce::Colours::black);
    constexpr int segments = 22;
    const float db = juce::Decibels::gainToDecibels(peak, -60.0f);
    const int lit = juce::jlimit(0, segments, juce::roundToInt((db + 60.0f) / 60.0f * segments));
    const float cw = (bar.getWidth() - 8.0f) / segments;
    for (int k = 0; k < segments; ++k)
    {
        const juce::Rectangle<float> seg { bar.getX() + 4.0f + k * cw, bar.getY() + 4.0f, cw - 2.0f, bar.getHeight() - 8.0f };
        if (k < lit)
        {
            g.setColour(pal.accent.withAlpha(0.25f));
            g.fillRect(seg.expanded(1.5f));
            g.setColour(pal.accent);
        }
        else g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRect(seg);
    }
    for (const int mark : { -60, -48, -36, -24, -12, -6, 0 })
    {
        const float x = bar.getX() + 4.0f + (mark + 60) / 60.0f * (bar.getWidth() - 8.0f);
        g.setColour(juce::Colours::black);
        g.fillRect(x, bar.getBottom() + 2.0f, 1.0f, 4.0f);
        drawText(g, juce::String(mark), juce::Rectangle<float>(26.0f, 12.0f).withCentre({ x, bar.getBottom() + 13.0f }),
                 pixelFont(8.0f), juce::Colours::black, juce::Justification::centred);
    }
}

// -------------------------------------------------------------------------------------- BUTTONS
ChoiceButton::ChoiceButton(Kind k, int i, const juce::String& label, juce::RangedAudioParameter& parameter)
    : juce::Button(label), kind(k), index(i),
      attachment(parameter, [this](float value) { selected = juce::roundToInt(value) == index; repaint(); }, nullptr)
{
    setWantsKeyboardFocus(false);
    setTooltip(label);
    onClick = [this] { attachment.setValueAsCompleteGesture(static_cast<float>(index)); };
    attachment.sendInitialUpdate();
}

void ChoiceButton::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto r = getLocalBounds().toFloat();
    const bool pressed = selected || down;
    button98(g, r, pressed);
    const float nudge = pressed ? 1.0f : 0.0f;
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
    // Mode button: pixel glyph (drawn small, scaled up nearest-neighbour) over its name.
    juce::Image icon(juce::Image::ARGB, 18, 12, true);
    {
        juce::Graphics ig(icon);
        ig.addTransform(juce::AffineTransform::scale(18.0f / 40.0f, 12.0f / 26.0f));
        ig.setColour(selected ? paletteFor(character).accent : juce::Colours::black);
        ig.strokePath(modeGlyph(index, { 4.0f, 2.0f, 32.0f, 22.0f }), juce::PathStrokeType(4.5f));
    }
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImage(icon, juce::Rectangle<float>(36.0f, 24.0f).withCentre(r.getCentre().translated(nudge, -6.0f)));
    drawText(g, getButtonText(), r.withTrimmedTop(r.getHeight() - 15.0f).translated(nudge, -1.0f), pixelFont(9.0f),
             juce::Colours::black, juce::Justification::centred);
}

// ------------------------------------------------------------------------------------- MENU BAR
juce::Rectangle<float> MenuBar98::itemBounds(int index) const
{
    float x = 2.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float w = juce::GlyphArrangement::getStringWidth(pixelFont(9.5f), items[i]) + 12.0f;
        if (i == index) return { x, 0.0f, w, 14.0f };
        x += w;
    }
    return {};
}

void MenuBar98::paint(juce::Graphics& g)
{
    for (int i = 0; i < 5; ++i)
    {
        const auto r = itemBounds(i);
        drawText(g, items[i], r, pixelFont(9.5f), juce::Colours::black, juce::Justification::centred);
        g.setColour(juce::Colours::black);
        g.fillRect(r.getX() + 6.0f, 11.0f, 5.0f, 1.0f);                  // mnemonic underline
    }
    g.setColour(win98::shadow);
    g.fillRect(0.0f, 15.0f, static_cast<float>(getWidth()), 1.0f);
    g.setColour(win98::light);
    g.fillRect(0.0f, 16.0f, static_cast<float>(getWidth()), 1.0f);
}

void MenuBar98::mouseDown(const juce::MouseEvent& e)
{
    for (int i = 0; i < 5; ++i)
    {
        const auto r = itemBounds(i);
        if (! r.contains(e.position)) continue;
        juce::PopupMenu menu;
        if (i == 2)
        {
            const int current = currentVoice ? currentVoice() : 1;
            menu.addItem(2, "SQUEAL", true, current == 1);
            menu.addItem(1, "BURP", true, current == 0);
            menu.addItem(3, "GROAN", true, current == 2);
        }
        else if (i == 4)
        {
            menu.addItem(100, "PHOQER Test UI", false, false);
            menu.addItem(101, "Experimental editor - not the release UI", false, false);
        }
        else
            menu.addItem(200, "Not available in the test UI", false, false);
        const auto target = localAreaToGlobal(r.toNearestInt());
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(target),
                           [safe = juce::Component::SafePointer<MenuBar98>(this)](int result)
                           {
                               if (safe != nullptr && result >= 1 && result <= 3 && safe->onVoiceChosen)
                                   safe->onVoiceChosen(result - 1);
                           });
        return;
    }
}

// -------------------------------------------------------------------------------------- TASKBAR
void Taskbar98::setMidiActive(bool active)
{
    if (active == midiActive) return;
    midiActive = active;
    repaint();
}

void Taskbar98::tickClock()
{
    const auto text = juce::Time::getCurrentTime().formatted("%I:%M %p").trimCharactersAtStart("0");
    if (text != clock) { clock = text; repaint(); }
}

void Taskbar98::paint(juce::Graphics& g)
{
    const auto& pal = paletteFor(character);
    const auto r = getLocalBounds().toFloat();
    g.setColour(win98::face);
    g.fillRect(r);
    g.setColour(win98::light);
    g.fillRect(r.getX(), r.getY() + 1.0f, r.getWidth(), 1.5f);
    const juce::Rectangle<float> start { 4.0f, 4.0f, 92.0f, 22.0f };
    button98(g, start, false);
    g.setColour(pal.accent);
    g.fillEllipse(juce::Rectangle<float>(12.0f, 10.0f).withCentre({ start.getX() + 14.0f, start.getCentreY() + 1.0f }));
    drawText(g, "PHOQER", start.withTrimmedLeft(26.0f), pixelFont(11.0f, true), juce::Colours::black);
    const char* voices[] { "BURP.BMP", "SQUEAL.BMP", "GROAN.BMP" };
    const char* tasks[] { "PHOQER.EXE", voices[juce::jlimit(0, 2, character)], "SCOPE.EXE", "MIXER.EXE" };
    float x = 104.0f;
    for (int i = 0; i < 4; ++i)
    {
        const juce::Rectangle<float> b { x, 4.0f, 118.0f, 22.0f };
        button98(g, b, i == 0);
        drawText(g, tasks[i], b.reduced(8.0f, 0.0f), pixelFont(9.0f, true), juce::Colours::black);
        x += 122.0f;
    }
    const juce::Rectangle<float> tray { r.getWidth() - 140.0f, 4.0f, 136.0f, 22.0f };
    bevel(g, tray, false);
    // MIDI note: lit while the seal is sounding.
    g.setColour(midiActive ? pal.accent : win98::shadow);
    g.fillRect(tray.getX() + 10.0f, tray.getY() + 13.0f, 5.0f, 4.0f);
    g.fillRect(tray.getX() + 14.0f, tray.getY() + 5.0f, 1.5f, 10.0f);
    g.fillRect(tray.getX() + 14.0f, tray.getY() + 5.0f, 5.0f, 2.0f);
    drawText(g, "MIDI", { tray.getX() + 24.0f, tray.getY(), 40.0f, tray.getHeight() }, pixelFont(8.5f), juce::Colours::black);
    drawText(g, clock, tray.reduced(6.0f, 0.0f), pixelFont(9.5f), juce::Colours::black, juce::Justification::centredRight);
}

// ---------------------------------------------------------------------------------------- PIANO
PianoView::PianoView(juce::MidiKeyboardState& state) : juce::MidiKeyboardComponent(state, horizontalKeyboard)
{
    // The editor owns computer-keyboard playing; this component only handles the mouse.
    clearKeyMappings();
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    setScrollButtonsVisible(false);
    setBlackNoteLengthProportion(0.6f);
    setBlackNoteWidthProportion(0.62f);
    setVelocity(0.9f, true);
    setOctaveForMiddleC(4);
    setColour(whiteNoteColourId, win98::dark);                  // shows through the 1 px gaps between keys
    setColour(keySeparatorLineColourId, win98::dark);
    setColour(shadowColourId, juce::Colours::transparentBlack);
    setQwertyBase(60);
}

void PianoView::setQwertyBase(int note)
{
    qwertyBase = note;
    setAvailableRange(note - 12, note + 24);
    resized();
    repaint();
}

void PianoView::resized()
{
    setKeyWidth(static_cast<float>(getWidth()) / static_cast<float>(whiteKeysShown));
    setLowestVisibleKey(qwertyBase - 12);
}

juce::String PianoView::keyLabel(int note) const
{
    const int i = note - qwertyBase;
    return juce::isPositiveAndBelow(i, qwertyKeyCount) ? juce::String::charToString(qwertyKeys[i]).toUpperCase() : juce::String();
}

void PianoView::drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over, juce::Colour, juce::Colour)
{
    const auto& pal = paletteFor(character);
    const auto key = area.withTrimmedRight(1.0f);
    g.setColour(down ? pal.accent : over ? juce::Colour(0xffe4e4ec) : juce::Colour(0xfff2f2f2));
    g.fillRect(key);
    if (! down)
    {
        g.setColour(win98::face);                              // pixel bevel at the key's base
        g.fillRect(key.withTop(key.getBottom() - 4.0f));
        g.setColour(win98::shadow);
        g.fillRect(key.withTop(key.getBottom() - 2.0f));
    }
    const auto ink = down ? juce::Colours::white : juce::Colours::black;
    if (const auto label = keyLabel(note); label.isNotEmpty())
        drawText(g, label, key.withTop(key.getBottom() - 18.0f).withTrimmedBottom(5.0f), pixelFont(9.0f, true), ink, juce::Justification::centred);
    if (note % 12 == 0)
        drawText(g, "C" + juce::String(note / 12 - 1), key.withTop(key.getBottom() - 30.0f).withHeight(10.0f), pixelFont(7.0f),
                 ink.withAlpha(0.55f), juce::Justification::centred);
}

void PianoView::drawBlackNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over, juce::Colour)
{
    const auto& pal = paletteFor(character);
    g.setColour(win98::dark);
    g.fillRect(area);
    const auto top = area.reduced(1.0f, 0.0f).withTrimmedBottom(1.0f);
    g.setColour(down ? pal.accent.brighter(0.2f) : over ? juce::Colour(0xff4a4458) : juce::Colour(0xff2a2632));
    g.fillRect(top);
    if (! down)
    {
        g.setColour(juce::Colour(0xff5c5868));                 // raised highlight on the key face
        g.fillRect(top.withWidth(2.0f).withTrimmedBottom(4.0f));
    }
    if (const auto label = keyLabel(note); label.isNotEmpty())
        drawText(g, label, top.withTop(top.getBottom() - 14.0f), pixelFont(8.0f, true),
                 down ? juce::Colours::black : juce::Colours::white.withAlpha(0.8f), juce::Justification::centred);
}
}
