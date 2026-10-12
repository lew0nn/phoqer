#pragma once

#include <JuceHeader.h>

#include <atomic>

namespace phoqer::ui
{
// Records PHOQER's output to a 24-bit WAV. The audio thread hands blocks to JUCE's ThreadedWriter
// (a lock-free FIFO drained to disk on a background thread); the take goes to a temporary file
// until the user saves or discards it.
class Recorder
{
public:
    Recorder();
    ~Recorder();

    bool start(double sampleRate, int numChannels, juce::int64 countInSamples = 0);    // count-in: not recorded
    juce::File stop();                                   // the finished temporary file
    bool isRecording() const noexcept { return active.load() != nullptr; }
    double seconds() const noexcept;
    double sampleRate() const noexcept { return rate; }
    juce::int64 countInRemaining() const noexcept { return isRecording() ? countIn.load() : 0; }

    void process(const juce::AudioBuffer<float>&);      // audio thread

    static juce::File folder();                          // Documents/PHOQER/Recordings
    static juce::File nextFileName(const juce::String& voice);

private:
    juce::TimeSliceThread thread { "PHOQER recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> active { nullptr };
    std::atomic<juce::int64> samplesWritten { 0 };
    std::atomic<juce::int64> countIn { 0 };
    double rate = 48000.0;
    juce::File temp;
};
}
