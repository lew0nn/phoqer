#pragma once

namespace phoqer
{
// sustainPedal: value 1 down, 0 up (CC 64). modWheel: value 0..1 (CC 1).
enum class MidiEventType { noteOn, noteOff, pitchWheel, allNotesOff, sustainPedal, modWheel };

struct MidiEvent
{
    MidiEventType type = MidiEventType::noteOn;
    int sampleOffset = 0;
    int channel = 0;
    int note = 60;
    float value = 0.0f;
};
}
