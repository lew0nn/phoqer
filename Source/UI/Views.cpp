#include "Views.h"

#include "../Core/FishSprite.h"

#include "Style.h"

#include <cmath>
#include <vector>

namespace phoqer::ui
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
    if (spectrogramFrames > 0) --spectrogramFrames;
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

void ScopeView::showSpectrogram(double seconds)
{
    spectrogramFrames = juce::roundToInt(seconds * 30.0);     // counted in UI frames (30 a second)
}

// The last ~2.7 s of output as a spectrogram, 0 to 3.4 kHz with the highest at the top, in the
// voice's colours. Only while a fed seal is singing: it is what makes the fish visible.
void ScopeView::paintSpectrogram(juce::Graphics& g)
{
    constexpr int order = 11, size = 1 << order, columns = 150;
    const double rate = juce::jmax(8000.0, static_cast<double>(telemetry.getSampleRate()));
    const int topBin = juce::jmin(size / 2, static_cast<int>(3400.0 / rate * size));
    telemetry.copyWaveform(capture);
    static juce::dsp::FFT fft(order);
    std::vector<float> work(static_cast<size_t>(size) * 2);
    if (! spectrogram.isValid()) spectrogram = juce::Image(juce::Image::RGB, columns, topBin, false);
    const auto& pal = paletteFor(character);
    const juce::Colour ramp[] { juce::Colours::black, pal.accent.darker(1.2f), pal.accent, pal.secondary, juce::Colours::white };
    const int hop = static_cast<int>((TelemetryPublisher::waveformSize - size) / columns);
    for (int c = 0; c < columns; ++c)
    {
        std::fill(work.begin(), work.end(), 0.0f);
        for (int i = 0; i < size; ++i)
        {
            const float window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(i) / size);
            work[static_cast<size_t>(i)] = capture[static_cast<size_t>(c * hop + i)] * window;
        }
        fft.performFrequencyOnlyForwardTransform(work.data());
        for (int bin = 0; bin < topBin; ++bin)
        {
            const float db = juce::Decibels::gainToDecibels(work[static_cast<size_t>(bin)] / (size * 0.25f), -120.0f);
            const float v = juce::jlimit(0.0f, 0.999f, (db + 52.0f) / 22.0f) * 4.0f;     // the song sits at -33 to -44 dB
            const int k = static_cast<int>(v);
            spectrogram.setPixelAt(c, topBin - 1 - bin, ramp[k].interpolatedWith(ramp[juce::jmin(4, k + 1)], v - static_cast<float>(k)));
        }
    }
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImage(spectrogram, getLocalBounds().toFloat());
}

void ScopeView::paint(juce::Graphics& g)
{
    if (spectrogramFrames > 0)
    {
        paintSpectrogram(g);
        return;
    }
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
    for (int i = 0; i < itemCount; ++i)
    {
        const float w = std::round(juce::GlyphArrangement::getStringWidth(pixelFont(12.0f), items[i])) + 14.0f;
        if (i == index) return { x, 0.0f, w, 15.0f };
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
    g.fillRect(0.0f, 15.0f, static_cast<float>(getWidth()), 1.0f);
    g.setColour(win98::light);
    g.fillRect(0.0f, 16.0f, static_cast<float>(getWidth()), 1.0f);
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
    const float cx = std::floor(b.getCentreX()) + (down ? 1.0f : 0.0f), cy = std::floor(b.getCentreY()) + (down ? 1.0f : 0.0f);
    g.setColour(juce::Colours::black);
    for (int k = 0; k < 4; ++k)
        g.fillRect(left ? cx - 2.0f + static_cast<float>(k) : cx + 1.0f - static_cast<float>(k), cy - static_cast<float>(k), 1.0f, 1.0f + 2.0f * static_cast<float>(k));
}

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

void RecordControl98::setState(bool isRecording, double seconds)
{
    const int s = static_cast<int>(seconds);
    if (isRecording == recording && s == shownSeconds) return;
    recording = isRecording;
    shownSeconds = s;
    repaint();
}

void RecordControl98::paint(juce::Graphics& g)
{
    const auto b = buttonBounds();
    button98(g, b, recording);                                         // stays pressed in while recording
    const float o = recording ? 1.0f : 0.0f;
    g.setColour(recording ? juce::Colour(0xffe61428) : juce::Colour(0xff783240));
    g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({ b.getX() + 10.0f + o, b.getCentreY() + o }));
    drawText(g, "REC", b.withTrimmedLeft(17.0f).translated(o, o), pixelFont(12.0f), juce::Colours::black);
    const auto lcd = getLocalBounds().toFloat().withTrimmedLeft(56.0f);
    sunken(g, lcd, juce::Colour(0xff10080c));
    const auto t = juce::String(shownSeconds / 60).paddedLeft('0', 2) + ":" + juce::String(shownSeconds % 60).paddedLeft('0', 2);
    drawText(g, t, lcd, logoFont(8.0f), recording ? juce::Colour(0xffff5064) : juce::Colour(0xff6e3c46), juce::Justification::centred);
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
    // Pixel glyphs as Win98 drew them, nudged down-right while pressed.
    const float cx = std::floor(b.getCentreX()) + (down ? 1.0f : 0.0f), cy = std::floor(b.getCentreY()) + (down ? 1.0f : 0.0f);
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
    if (getWidth() > 0)
        setKeyWidth(static_cast<float>(getWidth()) / static_cast<float>(whiteKeysShown));
    setLowestVisibleKey(qwertyBase - 12);
    juce::MidiKeyboardComponent::resized();
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
    drawText(g, getButtonText(), r.translated(down ? 1.0f : 0.0f, down ? 1.0f : 0.0f), pixelFont(12.0f, true), juce::Colours::black,
             juce::Justification::centred);
}

// --------------------------------------------------------------------------------------- DIALOG
namespace
{
constexpr float dialogWidth = 360.0f, dialogTitle = 20.0f, lineHeight = 16.0f, fieldHeight = 22.0f, buttonW = 76.0f, buttonH = 24.0f;
constexpr float headingHeight = 26.0f, rowHeight = 21.0f, gapHeight = 8.0f;

float heightOf(const juce::String& line) noexcept
{
    if (line.isEmpty()) return gapHeight;
    if (line.startsWith("#")) return headingHeight;
    if (line.containsChar('|')) return rowHeight;
    return lineHeight;
}

// "[CTRL]+[Z]": each bracketed key a small raised Win98 keycap, anything between them plain text.
void drawKeys(juce::Graphics& g, const juce::String& keys, float x, float y)
{
    const auto capFont = pixelFont(10.0f, true), joinFont = pixelFont(12.0f);
    for (int i = 0; i < keys.length();)
    {
        if (keys[i] == '[')
        {
            const int end = keys.indexOfChar(i, ']');
            if (end < 0) break;
            const auto label = keys.substring(i + 1, end);
            const float w = std::max(18.0f, juce::GlyphArrangement::getStringWidth(capFont, label) + 12.0f);
            const juce::Rectangle<float> cap { x, y + 1.0f, w, rowHeight - 4.0f };
            g.setColour(win98::face);
            g.fillRect(cap);
            bevel(g, cap, true);
            drawText(g, label, cap.translated(0.0f, -1.0f), capFont, juce::Colours::black, juce::Justification::centred);
            x += w + 3.0f;
            i = end + 1;
        }
        else
        {
            const auto join = juce::String::charToString(keys[i]);
            if (join != " ")
            {
                const float w = juce::GlyphArrangement::getStringWidth(joinFont, join) + 4.0f;
                drawText(g, join, { x, y, w, rowHeight - 2.0f }, joinFont, juce::Colours::black, juce::Justification::centred);
                x += w + 2.0f;
            }
            else
                x += 3.0f;
            ++i;
        }
    }
}
}

float Dialog98::linesHeight() const
{
    float h = 0.0f;
    for (const auto& line : spec.lines) h += heightOf(line);
    return h;
}

Dialog98::Dialog98(Spec s) : spec(std::move(s))
{
    setWantsKeyboardFocus(true);
    if (spec.fieldLabel.isNotEmpty())
    {
        field.setFont(pixelFont(12.0f));
        field.setText(spec.fieldText, false);
        field.setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
        field.setColour(juce::TextEditor::textColourId, juce::Colours::black);
        field.setColour(juce::TextEditor::highlightColourId, paletteFor(spec.character).accent.darker(0.35f));
        field.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        field.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        field.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        field.setColour(juce::CaretComponent::caretColourId, juce::Colours::black);
        field.setIndents(4, 4);
        field.onReturnKey = [this] { finish(0); };
        field.onEscapeKey = [this] { if (spec.closable) finish(-1); };
        addAndMakeVisible(field);
    }
    for (int i = 0; i < spec.buttons.size(); ++i)
    {
        auto b = std::make_unique<PushButton98>(spec.buttons[i], i == 0);
        b->onClick = [this, i] { finish(i); };
        addAndMakeVisible(*b);
        buttons.push_back(std::move(b));
    }
    closeButton.onClick = [this] { finish(-1); };
    addChildComponent(closeButton);
    closeButton.setVisible(spec.closable);
}

void Dialog98::parentHierarchyChanged()
{
    if (auto* p = getParentComponent())
    {
        setBounds(p->getLocalBounds());
        if (spec.fieldLabel.isNotEmpty()) { field.grabKeyboardFocus(); field.selectAll(); }
        else grabKeyboardFocus();
    }
}

juce::Rectangle<float> Dialog98::boxBounds() const
{
    const bool hasField = spec.fieldLabel.isNotEmpty();
    const float body = std::max(spec.icon.isValid() ? static_cast<float>(spec.icon.getHeight()) * 2.0f : 0.0f,
                                linesHeight() + (hasField ? fieldHeight + 10.0f : 0.0f));
    const float h = 4.0f + dialogTitle + 16.0f + body + 18.0f + buttonH + 14.0f;
    return juce::Rectangle<float>(std::max(dialogWidth, spec.width), h).withCentre(getLocalBounds().toFloat().getCentre()).toNearestInt().toFloat();
}

void Dialog98::resized()
{
    const auto box = boxBounds();
    closeButton.setBounds(juce::Rectangle<float>(box.getRight() - 4.0f - 2.0f - 16.0f, box.getY() + 6.0f, 16.0f, 14.0f).toNearestInt());
    const float textX = box.getX() + 16.0f + (spec.icon.isValid() ? static_cast<float>(spec.icon.getWidth()) * 2.0f + 16.0f : 0.0f);
    if (spec.fieldLabel.isNotEmpty())
    {
        const float y = box.getY() + 4.0f + dialogTitle + 16.0f + linesHeight();
        const float labelW = juce::GlyphArrangement::getStringWidth(pixelFont(12.0f), spec.fieldLabel) + 10.0f;
        field.setBounds(juce::Rectangle<float>(textX + labelW + 2.0f, y + 2.0f, box.getRight() - 18.0f - textX - labelW - 4.0f, fieldHeight - 4.0f).toNearestInt());
    }
    const float total = static_cast<float>(buttons.size()) * buttonW + static_cast<float>(buttons.size() - 1) * 8.0f;
    float bx = std::round(box.getCentreX() - total * 0.5f);
    for (auto& b : buttons)
    {
        b->setBounds(juce::Rectangle<float>(bx, box.getBottom() - 14.0f - buttonH, buttonW, buttonH).toNearestInt());
        bx += buttonW + 8.0f;
    }
}

void Dialog98::paint(juce::Graphics& g)
{
    const auto box = boxBounds();
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(box.translated(4.0f, 4.0f));
    g.setColour(win98::face);
    g.fillRect(box);
    bevel(g, box, true);
    const juce::Rectangle<float> bar { box.getX() + 4.0f, box.getY() + 4.0f, box.getWidth() - 8.0f, dialogTitle };
    g.setColour(paletteFor(spec.character).accent.darker(0.35f));
    g.fillRect(bar);
    drawText(g, spec.title, bar.reduced(5.0f, 0.0f), pixelFont(12.0f, true), juce::Colours::white);

    float x = box.getX() + 16.0f, y = bar.getBottom() + 16.0f;
    if (spec.icon.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.setOpacity(1.0f);
        const float iw = static_cast<float>(spec.icon.getWidth()) * 2.0f, ih = static_cast<float>(spec.icon.getHeight()) * 2.0f;
        g.drawImage(spec.icon, { x, y, iw, ih });
        x += iw + 16.0f;
    }
    const auto ink = paletteFor(spec.character).accent.darker(0.35f);
    const float right = box.getRight() - 16.0f;
    for (int i = 0; i < spec.lines.size(); ++i)
    {
        const auto& line = spec.lines[i];
        if (line.startsWith("#"))                               // a section heading, then an etched rule to the edge
        {
            const auto text = line.substring(1);
            const auto font = pixelFont(12.0f, true);
            const float ty = y + 6.0f, tw = juce::GlyphArrangement::getStringWidth(font, text);
            drawText(g, text, { x, ty, tw + 2.0f, lineHeight }, font, ink);
            const float ry = std::round(ty + lineHeight * 0.5f);
            g.setColour(win98::shadow); g.fillRect(x + tw + 8.0f, ry, right - x - tw - 8.0f, 1.0f);
            g.setColour(win98::light);  g.fillRect(x + tw + 8.0f, ry + 1.0f, right - x - tw - 8.0f, 1.0f);
        }
        else if (line.containsChar('|'))                        // two columns: a name or keys, then what it does
        {
            const auto left = line.upToFirstOccurrenceOf("|", false, false), text = line.fromFirstOccurrenceOf("|", false, false);
            if (left.startsWith("["))
                drawKeys(g, left, x, y);
            else
                drawText(g, left, { x, y + 2.0f, spec.labelWidth - 6.0f, lineHeight }, pixelFont(12.0f, true), ink);
            drawText(g, text, { x + spec.labelWidth, y + 2.0f, right - x - spec.labelWidth, lineHeight }, pixelFont(12.0f), juce::Colours::black);
        }
        else if (line.isNotEmpty())
        {
            const bool quiet = line.startsWith("~");            // "~" marks a secondary (grey) line
            drawText(g, quiet ? line.substring(1) : line, { x, y, right - x, lineHeight }, pixelFont(12.0f, i == 0 && ! quiet && spec.icon.isValid()),
                     quiet ? win98::shadow : juce::Colours::black);
        }
        y += heightOf(line);
    }
    if (spec.fieldLabel.isNotEmpty())
    {
        drawText(g, spec.fieldLabel, { x, y, 100.0f, fieldHeight }, pixelFont(12.0f), juce::Colours::black);
        sunken(g, field.getBounds().toFloat().expanded(2.0f));
    }
}

bool Dialog98::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey) { finish(0); return true; }
    if (key == juce::KeyPress::escapeKey) { if (spec.closable) finish(-1); return true; }
    return true;                                               // a modal box swallows everything else
}

void Dialog98::finish(int button)
{
    if (finished) return;
    finished = true;
    auto callback = spec.onClose;
    const auto text = field.getText().trim();
    // The owner deletes this dialog in the callback, so run it after this call has returned.
    juce::MessageManager::callAsync([callback, button, text] { if (callback) callback(button, text); });
}
}
