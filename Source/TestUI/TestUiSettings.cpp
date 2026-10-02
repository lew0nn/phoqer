#include "TestUiSettings.h"

namespace phoqer::testui
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
    options.applicationName = "PHOQER Test UI";
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
bool UiSettings::startFullscreen() const { return file->getBoolValue("startFullscreen", false); }
void UiSettings::setStartFullscreen(bool b) { file->setValue("startFullscreen", b); }
bool UiSettings::showKeys() const { return file->getBoolValue("showKeys", true); }
void UiSettings::setShowKeys(bool b) { file->setValue("showKeys", b); }
}
