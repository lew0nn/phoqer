#pragma once

#include <JuceHeader.h>

#include "../Core/FaceTelemetry.h"
#include "TestUiPortrait.h"

#include <array>
#include <functional>

namespace phoqer::testui
{
// SEAL.BMP client area: the expressive portrait bitmap, scanlines and call-driven glitch slices.
class SealView final : public juce::Component
{
public:
    void update(const PortraitRenderer&, int character, const Expression&);
    void paint(juce::Graphics&) override;

private:
    juce::Image bitmap;
    float intensity = 0.0f;
    int character = 1;
};

// SCOPE.EXE client area: captures the last call from the telemetry ring and draws it as pixel columns.
class ScopeView final : public juce::Component
{
public:
    explicit ScopeView(const TelemetryPublisher&);
    void setCharacter(int c) { character = c; repaint(); }
    void refresh();
    void paint(juce::Graphics&) override;
    float capturedMilliseconds() const noexcept { return capturedMs; }

private:
    enum class CaptureState { idle, capturing, holding, decaying };
    const TelemetryPublisher& telemetry;
    static constexpr int bucketCount = 1200;
    std::array<float, bucketCount> positive {}, negative {};
    std::array<float, TelemetryPublisher::waveformSize> capture {};
    uint32_t lastWriteIndex = 0, lastCallSerial = 0;
    int usedBuckets = 0, samplesInBucket = 0, samplesPerBucket = 48;
    double silentSeconds = 0.0, holdSeconds = 0.0;
    float displayGain = 1.0f, traceOpacity = 0.0f, capturePeak = 0.0f, capturedMs = 0.0f;
    CaptureState captureState = CaptureState::idle;
    int character = 1;
};

// OUTPUT meter: chunky segment bar with a dB scale.
class MeterView final : public juce::Component
{
public:
    explicit MeterView(const TelemetryPublisher& t) : telemetry(t) {}
    void setCharacter(int c) { character = c; repaint(); }
    void refresh();
    void paint(juce::Graphics&) override;

private:
    const TelemetryPublisher& telemetry;
    float peak = 0.0f;
    int character = 1;
};

// A Win98 push button bound to a choice parameter value (voice tab or mode button).
class ChoiceButton final : public juce::Button
{
public:
    enum class Kind { voiceTab, mode };
    ChoiceButton(Kind, int index, const juce::String& label, juce::RangedAudioParameter&);
    void setCharacter(int c) { character = c; repaint(); }
    void paintButton(juce::Graphics&, bool over, bool down) override;
    bool isSelected() const noexcept { return selected; }

private:
    Kind kind;
    int index;
    bool selected = false;
    int character = 1;
    juce::ParameterAttachment attachment;
};

// Menu bar with Win98 mnemonics. Voice and Help open real menus; the rest are disabled in this build.
class MenuBar98 final : public juce::Component
{
public:
    std::function<void(int)> onVoiceChosen;     // character index
    std::function<int()> currentVoice;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> itemBounds(int index) const;
    static constexpr const char* items[] { "File", "Edit", "Voice", "Preset", "Help" };
};

// Taskbar: Start button, (decorative) task buttons, tray with MIDI activity and the clock.
class Taskbar98 final : public juce::Component
{
public:
    void setCharacter(int c) { character = c; repaint(); }
    void setMidiActive(bool active);
    void tickClock();
    void paint(juce::Graphics&) override;

private:
    int character = 1;
    bool midiActive = false;
    juce::String clock;
};
}
