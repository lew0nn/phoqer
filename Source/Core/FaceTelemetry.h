#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

namespace phoqer
{
static_assert(std::atomic<float>::is_always_lock_free,
              "PHOQER telemetry requires lock-free float atomics");

struct FaceTelemetry
{
    float mouthOpen = 0.0f;
    float mouthRound = 0.0f;
    float jawWidth = 0.0f;
    float throatTension = 0.0f;
    float eyeSquint = 0.0f;
    float eyeOpen = 0.0f;
    float headLift = 0.0f;
    float intensity = 0.0f;
};

class TelemetryPublisher
{
public:
    static constexpr uint32_t waveformSize = 1024;

    TelemetryPublisher() noexcept
    {
        for (auto& sample : waveform)
            sample.store(0.0f, std::memory_order_relaxed);
    }

    void publishFace(const FaceTelemetry& value) noexcept
    {
        mouthOpen.store(value.mouthOpen, std::memory_order_relaxed);
        mouthRound.store(value.mouthRound, std::memory_order_relaxed);
        jawWidth.store(value.jawWidth, std::memory_order_relaxed);
        throatTension.store(value.throatTension, std::memory_order_relaxed);
        eyeSquint.store(value.eyeSquint, std::memory_order_relaxed);
        eyeOpen.store(value.eyeOpen, std::memory_order_relaxed);
        headLift.store(value.headLift, std::memory_order_relaxed);
        intensity.store(value.intensity, std::memory_order_release);
    }

    FaceTelemetry readFace() const noexcept
    {
        FaceTelemetry value;
        value.intensity = intensity.load(std::memory_order_acquire);
        value.mouthOpen = mouthOpen.load(std::memory_order_relaxed);
        value.mouthRound = mouthRound.load(std::memory_order_relaxed);
        value.jawWidth = jawWidth.load(std::memory_order_relaxed);
        value.throatTension = throatTension.load(std::memory_order_relaxed);
        value.eyeSquint = eyeSquint.load(std::memory_order_relaxed);
        value.eyeOpen = eyeOpen.load(std::memory_order_relaxed);
        value.headLift = headLift.load(std::memory_order_relaxed);
        return value;
    }

    void pushWaveform(float sample) noexcept
    {
        const auto next = writeIndex.load(std::memory_order_relaxed) + 1u;
        waveform[next & (waveformSize - 1u)].store(sample, std::memory_order_relaxed);
        writeIndex.store(next, std::memory_order_release);
    }

    void copyWaveform(std::array<float, waveformSize>& destination) const noexcept
    {
        const auto end = writeIndex.load(std::memory_order_acquire);
        for (uint32_t i = 0; i < waveformSize; ++i)
            destination[i] = waveform[(end - waveformSize + 1u + i) & (waveformSize - 1u)]
                                 .load(std::memory_order_relaxed);
    }

    void publishMeter(float newPeak, float newRms) noexcept
    {
        peak.store(newPeak, std::memory_order_relaxed);
        rms.store(newRms, std::memory_order_release);
    }

    float getPeak() const noexcept { return peak.load(std::memory_order_relaxed); }
    float getRms() const noexcept { return rms.load(std::memory_order_acquire); }

private:
    std::atomic<float> mouthOpen { 0.0f }, mouthRound { 0.0f }, jawWidth { 0.0f };
    std::atomic<float> throatTension { 0.0f }, eyeSquint { 0.0f }, eyeOpen { 0.0f };
    std::atomic<float> headLift { 0.0f }, intensity { 0.0f };
    std::array<std::atomic<float>, waveformSize> waveform;
    std::atomic<uint32_t> writeIndex { waveformSize - 1u };
    std::atomic<float> peak { 0.0f }, rms { 0.0f };
};
}
