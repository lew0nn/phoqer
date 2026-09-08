#include "PhoqerComponents.h"

#include <algorithm>
#include <cmath>

namespace phoqer::ui
{

WaveformDisplay::WaveformDisplay(const TelemetryPublisher& source) : telemetry(source)
{
    lastWriteIndex = telemetry.getWaveformWriteIndex();
    lastCallSerial = telemetry.getCallSerial();
}
void WaveformDisplay::setTheme(const ThemeAssets& value) noexcept { theme = &value; repaint(); }
void WaveformDisplay::refresh()
{
    const double rate = juce::jmax(8000.0f, telemetry.getSampleRate());
    const auto end = telemetry.copyWaveform(capture);
    const auto serial = telemetry.getCallSerial();
    const bool newCall = serial != lastCallSerial;
    lastCallSerial = serial;
    const auto previousEnd = lastWriteIndex;
    uint32_t available = juce::jmin(end - previousEnd, TelemetryPublisher::waveformSize);
    const double delta = juce::jmin(0.1, available / rate);
    if (newCall || (captureState == CaptureState::idle && telemetry.getPeak() >= 0.0015f))
    {
        positive.fill(0.0f); negative.fill(0.0f);
        usedBuckets = 0; samplesInBucket = 0;
        samplesPerBucket = juce::jmax(1, juce::roundToInt(rate / 1000.0));
        available = juce::jmin(TelemetryPublisher::waveformSize,
            available + static_cast<uint32_t>(rate * 0.025));
        capturePeak = 0.02f; displayGain = 1.0f; traceOpacity = 1.0f;
        silentSeconds = 0.0; holdSeconds = 0.0;
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
                // Compact all earlier peaks instead of scrolling away the attack.
                for (int b = 0; b < bucketCount / 2; ++b)
                {
                    positive[b] = juce::jmax(positive[b * 2], positive[b * 2 + 1]);
                    negative[b] = juce::jmin(negative[b * 2], negative[b * 2 + 1]);
                }
                std::fill(positive.begin() + bucketCount / 2, positive.end(), 0.0f);
                std::fill(negative.begin() + bucketCount / 2, negative.end(), 0.0f);
                usedBuckets = bucketCount / 2;
                samplesPerBucket *= 2;
            }
            const float value = capture[i];
            positive[usedBuckets] = juce::jmax(positive[usedBuckets], value);
            negative[usedBuckets] = juce::jmin(negative[usedBuckets], value);
            recentPeak = juce::jmax(recentPeak, std::abs(value));
            if (++samplesInBucket >= samplesPerBucket) { ++usedBuckets; samplesInBucket = 0; }
        }
        capturePeak = juce::jmax(capturePeak, recentPeak);
        const float target = juce::jlimit(0.01f, 14.0f, 0.86f / capturePeak);
        // Reduce gain immediately for peaks above unity so the attack is never flattened.
        displayGain = target < displayGain ? target : displayGain + (target - displayGain) * 0.18f;
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
void WaveformDisplay::paint(juce::Graphics& g)
{
    if (theme == nullptr) return;
    const auto bounds = getLocalBounds().toFloat();
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    // Nine-slice the supplied frame: source corners and edge thickness stay fixed.
    const auto& frame = theme->waveFrame;
    constexpr int edge = 5;
    const int sx[] {0, edge, frame.getWidth() - edge, frame.getWidth()};
    const int sy[] {0, edge, frame.getHeight() - edge, frame.getHeight()};
    const int dx[] {0, edge, getWidth() - edge, getWidth()};
    const int dy[] {0, edge, getHeight() - edge, getHeight()};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            g.drawImage(frame, dx[col], dy[row], dx[col + 1] - dx[col], dy[row + 1] - dy[row],
                sx[col], sy[row], sx[col + 1] - sx[col], sy[row + 1] - sy[row]);
    const auto plot = bounds.reduced(12.0f, 12.0f);
    const int count = juce::jmin(bucketCount, usedBuckets + (samplesInBucket > 0 ? 1 : 0));
    if (count < 2) return;
    const int width = juce::jmax(2, juce::roundToInt(plot.getWidth()));
    const int columns = juce::jmax(2, (width * count + bucketCount - 1) / bucketCount);
    std::vector<juce::Point<float>> lows;
    juce::Path shape, upper, lower;
    for (int x = 0; x < columns; ++x)
    {
        const int first = x * count / columns;
        const int last = juce::jmin(count, juce::jmax(first + 1, (x + 1) * count / columns));
        float hi = 0.0f, lo = 0.0f;
        for (int b = first; b < last; ++b) { hi = juce::jmax(hi, positive[b]); lo = juce::jmin(lo, negative[b]); }
        const float px = plot.getX() + x * plot.getWidth() / (width - 1);
        const float py = plot.getCentreY() - juce::jmin(1.0f, hi * displayGain) * plot.getHeight() * 0.44f;
        const float ny = plot.getCentreY() - juce::jmax(-1.0f, lo * displayGain) * plot.getHeight() * 0.44f;
        if (x == 0) { shape.startNewSubPath(px, py); upper.startNewSubPath(px, py); lower.startNewSubPath(px, ny); }
        else { shape.lineTo(px, py); upper.lineTo(px, py); lower.lineTo(px, ny); }
        lows.push_back({px, ny});
    }
    for (auto it = lows.rbegin(); it != lows.rend(); ++it) shape.lineTo(*it);
    shape.closeSubPath();
    g.setColour(theme->accent.withAlpha(0.16f * traceOpacity));
    g.strokePath(shape, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(theme->accent.withAlpha(0.72f * traceOpacity));
    g.fillPath(shape);
    g.setColour(theme->accentBright.withAlpha(0.9f * traceOpacity));
    const juce::PathStrokeType edgeStroke(1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.strokePath(upper, edgeStroke); g.strokePath(lower, edgeStroke);
}

OutputMeter::OutputMeter(const TelemetryPublisher& source) : telemetry(source) {}
void OutputMeter::setTheme(const ThemeAssets& value) noexcept { theme = &value; repaint(); }
void OutputMeter::refresh()
{
    peak = juce::jmax(telemetry.getPeak(), peak * 0.88f);
    rms = juce::jmax(telemetry.getRms(), rms * 0.92f);
    repaint();
}
void OutputMeter::paint(juce::Graphics& g)
{
    if (theme == nullptr) return;
    const auto bounds = getLocalBounds().toFloat();
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(theme->meterOff, bounds);
    const float db = juce::Decibels::gainToDecibels(peak, -60.0f);
    const int lit = juce::jlimit(0, 21, juce::roundToInt((db + 60.0f) / 60.0f * 21.0f));
    if (lit > 0)
    {
        juce::Graphics::ScopedSaveState saved(g);
        const float sx = bounds.getWidth() / 305.0f, sy = bounds.getHeight() / 60.0f;
        g.reduceClipRegion(juce::Rectangle<float>(15.0f * sx, 9.0f * sy,
            lit * 13.0f * sx, 26.0f * sy).toNearestInt());
        g.drawImage(theme->meterOn, bounds);
    }
}

SealDisplay::SealDisplay(const TelemetryPublisher& source) : telemetry(source) {}
void SealDisplay::setTheme(const ThemeAssets& value) noexcept { theme = &value; repaint(); }
void SealDisplay::refresh() { face = telemetry.readFace(); repaint(); }
void SealDisplay::paint(juce::Graphics& g)
{
    if (theme == nullptr) return;
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    // The approved portrait already includes its sole mechanical frame.
    g.drawImage(theme->seal, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
}

ModeButton::ModeButton(int index, juce::RangedAudioParameter& parameter)
    : juce::Button("Mode " + juce::String(index + 1)), modeIndex(index),
      attachment(parameter, [this](float value)
      {
          selected = juce::roundToInt(value) == modeIndex;
          repaint();
      }, nullptr)
{
    setWantsKeyboardFocus(false);
    onClick = [this] { attachment.setValueAsCompleteGesture(static_cast<float>(modeIndex)); };
    attachment.sendInitialUpdate();
}
void ModeButton::setTheme(const ThemeAssets& value) noexcept { theme = &value; repaint(); }
void ModeButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    if (theme == nullptr) return;
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.setOpacity(down ? 0.90f : 1.0f);
    g.drawImage(theme->modeButtons[static_cast<size_t>(modeIndex)],
        getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
    if (selected || over)
    {
        g.setOpacity(selected ? 1.0f : 0.55f);
        g.drawImage(theme->led, juce::Rectangle<float>(7.0f, 7.0f)
            .withCentre({getWidth() * 0.5f, getHeight() * 0.82f}));
    }
}

MenuButton::MenuButton() : juce::Button("Menu") { setWantsKeyboardFocus(false); }
void MenuButton::setTheme(const ThemeAssets& value) noexcept { theme = &value; repaint(); }
void MenuButton::paintButton(juce::Graphics& g, bool, bool down)
{
    if (theme == nullptr) return;
    g.setOpacity(down ? 0.90f : 1.0f);
    g.drawImage(theme->menu, getLocalBounds().toFloat().reduced(5.0f), juce::RectanglePlacement::centred);
}
}
