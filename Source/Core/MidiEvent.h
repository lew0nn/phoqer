#pragma once

namespace phoqer
{
enum class MidiEventType { noteOn, noteOff, pitchWheel, allNotesOff };

struct MidiEvent
{
    MidiEventType type = MidiEventType::noteOn;
    int sampleOffset = 0;
    int channel = 0;
    int note = 60;
    float value = 0.0f;
};
}
