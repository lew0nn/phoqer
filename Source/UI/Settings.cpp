#include "Settings.h"

namespace phoqer::ui
{
namespace
{
UiSettings* instance = nullptr;
}

UiSettings& UiSettings::get()
{
    if (instance == nullptr) instance = new UiSettings();
    return *instance;
}

UiSettings::~UiSettings()
{
    file->saveIfNeeded();
    instance = nullptr;
}

UiSettings::UiSettings()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "PHOQER";
    options.folderName = "PHOQER";
   #if JUCE_LINUX || JUCE_BSD
    options.folderName = "~/.config/PHOQER";
   #endif
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.millisecondsBeforeSaving = 500;
    file = std::make_unique<juce::PropertiesFile>(options);
}

float UiSettings::zoom() const { return juce::jlimit(0.75f, 4.0f, static_cast<float>(file->getDoubleValue("zoom", 1.0))); }
void UiSettings::setZoom(float z) { file->setValue("zoom", static_cast<double>(z)); }
juce::String UiSettings::voiceOrder() const
{
    const auto order = file->getValue("voiceOrder", "0123");
    for (const auto c : juce::String("0123")) if (order.indexOfChar(c) < 0 || order.length() != 4) return "0123";     // not all four: the default
    return order;
}
void UiSettings::setVoiceOrder(const juce::String& order) { file->setValue("voiceOrder", order); }
bool UiSettings::startFullscreen() const { return file->getBoolValue("startFullscreen", false); }
void UiSettings::setStartFullscreen(bool b) { file->setValue("startFullscreen", b); }
bool UiSettings::checkUpdatesDaily() const { return file->getBoolValue("checkUpdatesDaily", true); }
void UiSettings::setCheckUpdatesDaily(bool b) { file->setValue("checkUpdatesDaily", b); }
juce::int64 UiSettings::lastUpdateCheck() const { return file->getValue("lastUpdateCheck", "0").getLargeIntValue(); }
void UiSettings::setLastUpdateCheck(juce::int64 t) { file->setValue("lastUpdateCheck", juce::String(t)); }
juce::String UiSettings::appFolder() const { return file->getValue("appFolder"); }
void UiSettings::setAppFolder(const juce::String& f) { if (f != appFolder()) file->setValue("appFolder", f); }
juce::String UiSettings::vst3Folder() const { return file->getValue("vst3Folder"); }
void UiSettings::setVst3Folder(const juce::String& f) { if (f != vst3Folder()) file->setValue("vst3Folder", f); }
bool UiSettings::showKeys() const { return file->getBoolValue("showKeys", true); }
void UiSettings::setShowKeys(bool b) { file->setValue("showKeys", b); }
}
