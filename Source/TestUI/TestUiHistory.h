#pragma once

#include <JuceHeader.h>

#include <functional>
#include <vector>

namespace phoqer::testui
{
// Undo / redo for the knobs, voice and mode. A step is recorded when a gesture made in the UI ends
// (a knob drag, a button click); performAsOne() records several changes (a preset load, a reset) as
// one step. Gestures from other threads, like host automation, are not recorded.
class ParamHistory final : private juce::AudioProcessorParameter::Listener
{
public:
    explicit ParamHistory(juce::AudioProcessor&);
    ~ParamHistory() override;

    void performAsOne(const std::function<void()>&);
    bool canUndo() const noexcept { return ! undoSteps.empty(); }
    bool canRedo() const noexcept { return ! redoSteps.empty(); }
    void undo();
    void redo();

private:
    using Snapshot = std::vector<float>;                   // normalised value of every parameter
    Snapshot capture() const;
    void restore(const Snapshot&);
    void record(const Snapshot& before);
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override;

    static constexpr size_t maxSteps = 100;
    juce::Array<juce::AudioProcessorParameter*> parameters;
    std::vector<Snapshot> undoSteps, redoSteps;
    Snapshot beforeGesture;
    int openGestures = 0;
    bool busy = false;
};
}
