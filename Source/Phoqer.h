#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "Core/MidiEvent.h"
#include "Core/PhoqerEngine.h"
#include "Core/PhoqerParameters.h"

#include <array>

using namespace iplug;
using namespace igraphics;

class Phoqer final : public Plugin
{
public:
    explicit Phoqer(const InstanceInfo& info);

#if IPLUG_DSP
    void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
    void ProcessMidiMsg(const IMidiMsg& msg) override;
    void OnReset() override;
#endif

private:
#if IPLUG_DSP
    void queueMidiEvent(const phoqer::MidiEvent& event) noexcept;

    phoqer::PhoqerEngine engine;
    std::array<phoqer::MidiEvent, 256> midiEvents {};
    int midiEventCount = 0;
#endif
};
