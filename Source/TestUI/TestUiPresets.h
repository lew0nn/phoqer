#pragma once

#include <JuceHeader.h>

#include <map>
#include <vector>

namespace phoqer::testui
{
// A sound: voice, mode and every knob except OUTPUT (loading a preset never changes your level).
struct Preset
{
    juce::String name;
    std::map<juce::String, float> values;      // parameter ID -> real value
    juce::File file;                           // empty for the built-ins
};

// Built-in presets plus the user's own (Documents/PHOQER/Presets/*.phqpreset), and which one is
// loaded. The loaded preset's name is kept in the plugin state so it comes back with the session.
class PresetLibrary
{
public:
    explicit PresetLibrary(juce::AudioProcessorValueTreeState&);

    static juce::File folder();
    static juce::StringArray parameterIds();

    void refresh();
    const std::vector<Preset>& all() const noexcept { return presets; }
    int currentIndex() const noexcept { return current; }

    void load(int index);                      // applies it to the parameters
    bool loadFile(const juce::File&);          // a preset file from anywhere
    bool save(const juce::String& name);       // the current sound, into the presets folder
    bool isDirty() const;                      // changed since it was loaded or saved
    juce::String displayName() const;          // "002  HARBOUR BARK" (+ " *" when changed)
    static juce::String numbered(int index, const juce::String& name);
    static juce::String cleanName(const juce::String&);   // upper case, letters, digits, space - _
    int find(const juce::String& name) const;          // -1 when there is none
    bool isBuiltIn(int index) const;

private:
    Preset capture(const juce::String& name) const;
    void apply(const Preset&);
    void remember(const juce::String& name);

    juce::AudioProcessorValueTreeState& state;
    std::vector<Preset> presets;
    Preset loaded;                             // what "unchanged" means for the star
    bool hasLoaded = false;
    int current = -1;
};
}
