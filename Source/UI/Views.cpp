// The displays: the seal portrait, SCOPE.EXE (waveform and spectrogram) and LEVEL.EXE's meter.

#include "Views.h"

#include "../Core/FishSprite.h"

#include "Style.h"

#include <cmath>
#include <cstring>
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
    // While the fish is on screen the waveform is not recorded: the song's loudness follows the
    // fish's outline, and capturing it would show a second, fish-shaped blob once the picture ends.
    if (spectrogramFrames > 0)
    {
        --spectrogramFrames;
        updateSpectrogram();
        lastWriteIndex = telemetry.getWaveformWriteIndex();
        lastCallSerial = telemetry.getCallSerial();
        captureState = CaptureState::idle;
        usedBuckets = 0;
        traceOpacity = 0.0f;
        repaint();
        return;
    }
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
    spectrogram = {};                                          // starts black; columns arrive as it sings
}

// The last ~2.7 s of output as a spectrogram, 0 to 3.4 kHz with the highest at the top, in the
// voice's colours. Only while a fed seal is singing: it is what makes the fish visible. Each frame
// adds just the one or two columns of sound that arrived since the last and slides the picture left,
// so it costs a couple of FFTs a frame, not 150.
void ScopeView::updateSpectrogram()
{
    constexpr int order = 11, size = 1 << order, columns = 150;
    constexpr uint32_t hop = (TelemetryPublisher::waveformSize - size) / columns;
    const double rate = juce::jmax(8000.0, static_cast<double>(telemetry.getSampleRate()));
    const int topBin = juce::jmin(size / 2, static_cast<int>(3400.0 / rate * size));
    static juce::dsp::FFT fft(order);
    if (fftWork.empty())
    {
        fftWork.resize(static_cast<size_t>(size) * 2);
        hann.resize(static_cast<size_t>(size));
        for (int i = 0; i < size; ++i)
            hann[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(i) / size);
    }

    const uint32_t end = telemetry.copyWaveform(capture);      // capture.back() is sample number `end`
    const uint32_t newest = (end + 1u - static_cast<uint32_t>(size)) / hop;   // the last column with all its samples
    if (! spectrogram.isValid() || spectrogram.getHeight() != topBin)
    {
        spectrogram = juce::Image(juce::Image::RGB, columns, topBin, true, juce::SoftwareImageType());     // plain memory: shifted by hand
        lastColumn = newest;
        return;
    }
    const int fresh = static_cast<int>(juce::jmin<uint32_t>(newest - lastColumn, columns));
    if (fresh <= 0) return;
    lastColumn = newest;
    const auto& pal = paletteFor(character);
    const juce::Colour ramp[] { juce::Colours::black, pal.accent.darker(1.2f), pal.accent, pal.secondary, juce::Colours::white };
    juce::Image::BitmapData pixels(spectrogram, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < topBin; ++y)     // slide the picture left by the new columns
        std::memmove(pixels.getLinePointer(y), pixels.getPixelPointer(fresh, y),
                     static_cast<size_t>((columns - fresh) * pixels.pixelStride));
    for (int k = 0; k < fresh; ++k)
    {
        const uint32_t column = newest - static_cast<uint32_t>(fresh - 1 - k);
        const uint32_t first = column * hop;                          // its first sample number
        const auto offset = static_cast<size_t>(TelemetryPublisher::waveformSize - 1u - (end - first));
        std::fill(fftWork.begin(), fftWork.end(), 0.0f);
        for (size_t i = 0; i < static_cast<size_t>(size); ++i)
            fftWork[i] = capture[offset + i] * hann[i];
        fft.performFrequencyOnlyForwardTransform(fftWork.data());
        const int x = columns - fresh + k;
        for (int bin = 0; bin < topBin; ++bin)
        {
            const float db = juce::Decibels::gainToDecibels(fftWork[static_cast<size_t>(bin)] / (size * 0.25f), -120.0f);
            const float v = juce::jlimit(0.0f, 0.999f, (db + 52.0f) / 22.0f) * 4.0f;     // the song sits at -33 to -44 dB
            const int step = static_cast<int>(v);
            pixels.setPixelColour(x, topBin - 1 - bin, ramp[step].interpolatedWith(ramp[juce::jmin(4, step + 1)], v - static_cast<float>(step)));
        }
    }
}

void ScopeView::paint(juce::Graphics& g)
{
    if (spectrogramFrames > 0)
    {
        if (spectrogram.isValid())
        {
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
            g.drawImage(spectrogram, getLocalBounds().toFloat());
        }
        else g.fillAll(juce::Colours::black);
        return;
    }
    const auto& pal = paletteFor(character);
    g.fillAll(juce::Colours::black);
    const auto plot = getLocalBounds().toFloat().reduced(8.0f, 8.0f);
    g.setColour(character==3?approvedHoloColour(paletteFor(2).accent.withMultipliedBrightness(.28f)):pal.accent.withAlpha(.28f));
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
}
