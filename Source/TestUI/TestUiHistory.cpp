#include "TestUiHistory.h"

namespace phoqer::testui
{
ParamHistory::ParamHistory(juce::AudioProcessor& processor) : parameters(processor.getParameters())
{
    for (auto* p : parameters) p->addListener(this);
}

ParamHistory::~ParamHistory()
{
    for (auto* p : parameters) p->removeListener(this);
}

ParamHistory::Snapshot ParamHistory::capture() const
{
    Snapshot s;
    s.reserve(static_cast<size_t>(parameters.size()));
    for (auto* p : parameters) s.push_back(p->getValue());
    return s;
}

void ParamHistory::restore(const Snapshot& s)
{
    const juce::ScopedValueSetter<bool> quiet(busy, true);
    for (int i = 0; i < parameters.size() && i < static_cast<int>(s.size()); ++i)
    {
        auto* p = parameters[i];
        const float value = s[static_cast<size_t>(i)];
        if (juce::approximatelyEqual(p->getValue(), value)) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost(value);
        p->endChangeGesture();
    }
}

void ParamHistory::record(const Snapshot& before)
{
    if (before == capture()) return;                       // a click that changed nothing
    undoSteps.push_back(before);
    if (undoSteps.size() > maxSteps) undoSteps.erase(undoSteps.begin());
    redoSteps.clear();
}

void ParamHistory::parameterGestureChanged(int, bool starting)
{
    if (busy || ! juce::MessageManager::existsAndIsCurrentThread()) return;
    if (starting)
    {
        if (openGestures++ == 0) beforeGesture = capture();
    }
    else if (openGestures > 0 && --openGestures == 0)
    {
        record(beforeGesture);
    }
}

void ParamHistory::performAsOne(const std::function<void()>& change)
{
    if (busy) { change(); return; }
    const auto before = capture();
    {
        const juce::ScopedValueSetter<bool> quiet(busy, true);
        change();
    }
    record(before);
}

void ParamHistory::undo()
{
    if (undoSteps.empty()) return;
    auto now = capture();
    restore(undoSteps.back());
    undoSteps.pop_back();
    redoSteps.push_back(std::move(now));
}

void ParamHistory::redo()
{
    if (redoSteps.empty()) return;
    auto now = capture();
    restore(redoSteps.back());
    redoSteps.pop_back();
    undoSteps.push_back(std::move(now));
}
}
