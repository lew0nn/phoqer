#include "TestUiPresets.h"

namespace phoqer::testui
{
namespace
{
constexpr auto fileExtension = ".phqpreset";
const juce::Identifier presetProperty { "testUiPreset" };

// Placeholder sound design for the built-ins: voice (0 burp, 1 squeal, 2 groan), mode (0 call,
// 1 honk, 2 bark, 3 wail, 4 murmur) and the seven sound knobs.
struct BuiltIn { const char* name; int voice, mode; float boom, air, bark, space, vowel, detune, tide; };
constexpr BuiltIn builtIns[] {
    { "INIT",          0, 0, 0.50f, 0.25f, 0.35f, 0.00f, 0.35f, 0.00f, 0.25f },
    { "HARBOUR BARK",  0, 2, 0.70f, 0.20f, 0.75f, 0.25f, 0.30f, 0.10f, 0.20f },
    { "BELLY RUMBLE",  0, 4, 0.90f, 0.10f, 0.30f, 0.15f, 0.20f, 0.20f, 0.50f },
    { "PUP CALL",      1, 0, 0.30f, 0.45f, 0.20f, 0.30f, 0.60f, 0.05f, 0.35f },
    { "ICE SHRIEK",    1, 1, 0.20f, 0.70f, 0.55f, 0.45f, 0.80f, 0.15f, 0.20f },
    { "NIGHT GROAN",   2, 3, 0.40f, 0.35f, 0.10f, 0.60f, 0.45f, 0.10f, 0.60f },
    { "FOG HORN",      2, 3, 0.65f, 0.15f, 0.20f, 0.50f, 0.25f, 0.30f, 0.15f },
};

bool same(float a, float b) { return std::abs(a - b) < 0.0015f; }
}

PresetLibrary::PresetLibrary(juce::AudioProcessorValueTreeState& s) : state(s)
{
    refresh();
    // Pick up the preset the session was using, without re-applying it.
    const auto name = state.state.getProperty(presetProperty, "INIT").toString();
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name) { current = static_cast<int>(i); loaded = presets[i]; hasLoaded = true; break; }
}

juce::File PresetLibrary::folder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("PHOQER").getChildFile("Presets");
}

juce::StringArray PresetLibrary::parameterIds()
{
    return { "character", "behavior", "boom", "air", "bark", "space", "vowel", "detune", "tide" };
}

void PresetLibrary::refresh()
{
    const auto currentName = current >= 0 && current < static_cast<int>(presets.size()) ? presets[static_cast<size_t>(current)].name : juce::String();
    presets.clear();
    for (const auto& b : builtIns)
    {
        Preset p;
        p.name = b.name;
        p.values = { { "character", static_cast<float>(b.voice) }, { "behavior", static_cast<float>(b.mode) }, { "boom", b.boom },
                     { "air", b.air }, { "bark", b.bark }, { "space", b.space }, { "vowel", b.vowel }, { "detune", b.detune }, { "tide", b.tide } };
        presets.push_back(p);
    }
    auto files = folder().findChildFiles(juce::File::findFiles, false, juce::String("*") + fileExtension);
    files.sort();
    for (const auto& f : files)
        if (auto xml = juce::parseXML(f); xml != nullptr && xml->hasTagName("PHOQERPRESET"))
        {
            Preset p;
            p.name = xml->getStringAttribute("name", f.getFileNameWithoutExtension()).toUpperCase();
            p.file = f;
            for (auto* e : xml->getChildWithTagNameIterator("PARAM"))
                p.values[e->getStringAttribute("id")] = static_cast<float>(e->getDoubleAttribute("value"));
            presets.push_back(p);
        }
    current = -1;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == currentName) current = static_cast<int>(i);
}

Preset PresetLibrary::capture(const juce::String& name) const
{
    Preset p;
    p.name = name;
    for (const auto& id : parameterIds())
        if (auto* param = state.getParameter(id))
            p.values[id] = param->convertFrom0to1(param->getValue());
    return p;
}

void PresetLibrary::apply(const Preset& p)
{
    for (const auto& [id, value] : p.values)
        if (auto* param = state.getParameter(id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(value));
            param->endChangeGesture();
        }
}

void PresetLibrary::remember(const juce::String& name)
{
    state.state.setProperty(presetProperty, name, nullptr);
}

void PresetLibrary::load(int index)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(presets.size()))) return;
    current = index;
    loaded = presets[static_cast<size_t>(index)];
    hasLoaded = true;
    apply(loaded);
    remember(loaded.name);
}

bool PresetLibrary::loadFile(const juce::File& f)
{
    auto xml = juce::parseXML(f);
    if (xml == nullptr || ! xml->hasTagName("PHOQERPRESET")) return false;
    Preset p;
    p.name = xml->getStringAttribute("name", f.getFileNameWithoutExtension()).toUpperCase();
    p.file = f;
    for (auto* e : xml->getChildWithTagNameIterator("PARAM"))
        p.values[e->getStringAttribute("id")] = static_cast<float>(e->getDoubleAttribute("value"));
    apply(p);
    loaded = p;
    hasLoaded = true;
    remember(p.name);
    refresh();
    return true;
}

juce::String PresetLibrary::cleanName(const juce::String& raw)
{
    return raw.toUpperCase().retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_").trim().substring(0, 24);
}

int PresetLibrary::find(const juce::String& name) const
{
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name) return static_cast<int>(i);
    return -1;
}

bool PresetLibrary::isBuiltIn(int index) const
{
    return juce::isPositiveAndBelow(index, static_cast<int>(presets.size())) && presets[static_cast<size_t>(index)].file == juce::File();
}

bool PresetLibrary::save(const juce::String& rawName)
{
    const auto name = cleanName(rawName);
    if (name.isEmpty()) return false;
    const auto p = capture(name);
    juce::XmlElement xml("PHOQERPRESET");
    xml.setAttribute("name", name);
    xml.setAttribute("version", 1);
    for (const auto& [id, value] : p.values)
    {
        auto* e = xml.createNewChildElement("PARAM");
        e->setAttribute("id", id);
        e->setAttribute("value", value);
    }
    folder().createDirectory();
    const auto file = folder().getChildFile(name + fileExtension);
    if (! xml.writeTo(file)) return false;
    loaded = p;
    hasLoaded = true;
    remember(name);
    refresh();
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name) current = static_cast<int>(i);
    return true;
}

bool PresetLibrary::isDirty() const
{
    if (! hasLoaded) return false;
    for (const auto& [id, value] : loaded.values)
        if (auto* param = state.getParameter(id); param != nullptr && ! same(param->convertFrom0to1(param->getValue()), value))
            return true;
    return false;
}

juce::String PresetLibrary::numbered(int index, const juce::String& name)
{
    return (index >= 0 ? juce::String(index + 1).paddedLeft('0', 3) : juce::String("---")) + "  " + name;
}

juce::String PresetLibrary::displayName() const
{
    if (! hasLoaded) return numbered(-1, "UNSAVED");
    return numbered(current, loaded.name) + (isDirty() ? " *" : "");
}
}
