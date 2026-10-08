#pragma once

namespace phoqer
{
// sustainPedal: value 1 down, 0 up (CC 64). modWheel: value 0..1 (CC 1).
// treat: the seal was fed the fish; it sings the fish (the UI secret). belch: overfed; it gulps
// and belches instead.
enum class MidiEventType { noteOn, noteOff, pitchWheel, allNotesOff, sustainPedal, modWheel, treat, belch };

struct MidiEvent
{
    MidiEventType type = MidiEventType::noteOn;
    int sampleOffset = 0;
    int channel = 0;
    int note = 60;
    float value = 0.0f;
};
}
