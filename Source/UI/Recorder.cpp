#include "Recorder.h"

namespace phoqer::ui
{
Recorder::Recorder() { thread.startThread(); }

Recorder::~Recorder()
{
    temp = stop();
    temp.deleteFile();
}

bool Recorder::start(double sampleRate, int numChannels)
{
    stop();
    rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    temp = juce::File::createTempFile(".wav");
    auto stream = std::make_unique<juce::FileOutputStream>(temp);
    if (! stream->openedOk()) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(stream.get(), rate, static_cast<unsigned int>(numChannels), 24, {}, 0));
    if (w == nullptr) return false;
    stream.release();                                    // the writer owns the stream now
    samplesWritten = 0;
    writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(w.release(), thread, 32768);
    const juce::ScopedLock sl(writerLock);
    active = writer.get();
    return true;
}

juce::File Recorder::stop()
{
    {
        const juce::ScopedLock sl(writerLock);
        active = nullptr;
    }
    writer.reset();                                      // flushes and closes the file
    return temp;
}

double Recorder::seconds() const noexcept
{
    return static_cast<double>(samplesWritten.load()) / rate;
}

void Recorder::process(const juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedLock sl(writerLock);
    if (auto* w = active.load())
        if (w->write(buffer.getArrayOfReadPointers(), buffer.getNumSamples()))
            samplesWritten += buffer.getNumSamples();
}

juce::File Recorder::folder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("PHOQER").getChildFile("Recordings");
}

juce::File Recorder::nextFileName(const juce::String& voice)
{
    for (int n = 1; n < 10000; ++n)
    {
        const auto f = folder().getChildFile("PHOQER-" + voice + "-" + juce::String(n).paddedLeft('0', 3) + ".WAV");
        if (! f.existsAsFile()) return f;
    }
    return folder().getChildFile("PHOQER-" + voice + ".WAV");
}
}
