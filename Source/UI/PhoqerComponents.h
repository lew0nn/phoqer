#pragma once

#include <JuceHeader.h>

#include "../Core/FaceTelemetry.h"
#include "PhoqerTheme.h"

#include <array>

namespace phoqer::ui
{
class WaveformDisplay final : public juce::Component
{
public:
    explicit WaveformDisplay(const TelemetryPublisher&);
    void setTheme(const ThemeAssets&) noexcept;
    void refresh();
    void paint(juce::Graphics&) override;

private:
    enum class CaptureState { idle, capturing, holding, decaying };

    const TelemetryPublisher& telemetry;
    const ThemeAssets* theme = nullptr;
    static constexpr int bucketCount = 1200;
    std::array<float, bucketCount> positive {}, negative {};
    uint32_t lastWriteIndex = 0, lastCallSerial = 0;
    int usedBuckets = 0, samplesInBucket = 0, samplesPerBucket = 44;
    double silentSeconds = 0.0, holdSeconds = 0.0;
    std::array<float, TelemetryPublisher::waveformSize> capture {};
    float displayGain = 1.0f;
    float traceOpacity = 0.0f;
    float capturePeak = 0.0f;
    CaptureState captureState = CaptureState::idle;
};

class OutputMeter final : public juce::Component
{
public:
    explicit OutputMeter(const TelemetryPublisher&);
    void setTheme(const ThemeAssets&) noexcept;
    void refresh();
    void paint(juce::Graphics&) override;

private:
    const TelemetryPublisher& telemetry;
    const ThemeAssets* theme = nullptr;
    float peak = 0.0f;
    float rms = 0.0f;
};

class SealDisplay final : public juce::Component
{
public:
    explicit SealDisplay(const TelemetryPublisher&);
    void setTheme(const ThemeAssets&) noexcept;
    void refresh();
    void paint(juce::Graphics&) override;

private:
    const TelemetryPublisher& telemetry;
    const ThemeAssets* theme = nullptr;
    FaceTelemetry face;
};

class ModeButton final : public juce::Button
{
public:
    ModeButton(int index, juce::RangedAudioParameter&);
    void setTheme(const ThemeAssets&) noexcept;
    void paintButton(juce::Graphics&, bool, bool) override;

private:
    int modeIndex;
    const ThemeAssets* theme = nullptr;
    bool selected = false;
    juce::ParameterAttachment attachment;
};

class MenuButton final : public juce::Button
{
public:
    MenuButton();
    void setTheme(const ThemeAssets&) noexcept;
    void paintButton(juce::Graphics&, bool, bool) override;

private:
    const ThemeAssets* theme = nullptr;
};
}
