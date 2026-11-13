#pragma once

#include <JuceHeader.h>

namespace phoqer::ui
{
// The editor's own preferences (view size and layout), shared by the standalone app and the plugin
// and kept in the user's application-data folder: PHOQER/PHOQER.settings. Created on first
// use and deleted with JUCE's other singletons (its save timer must not outlive the message thread).
class UiSettings final : private juce::DeletedAtShutdown
{
public:
    ~UiSettings() override;
    static UiSettings& get();

    float zoom() const;                    // 1.0 = 100%
    void setZoom(float);
    bool startFullscreen() const;
    void setStartFullscreen(bool);
    bool showKeys() const;
    void setShowKeys(bool);
    // The quick bar's order: the four voices (0 BURP .. 3 KYRAX) as you put them in it, the same for every song.
    juce::String voiceOrder() const;
    // HELP > CHECK EVERY DAY, and when PHOQER last read lewonn.com's update file (seconds since 1970).
    bool checkUpdatesDaily() const;
    void setCheckUpdatesDaily(bool);
    juce::int64 lastUpdateCheck() const;
    void setLastUpdateCheck(juce::int64);
    void setVoiceOrder(const juce::String&);
    // Where the standalone app and the VST3 were last run from, so an update replaces each copy where it is.
    juce::String appFolder() const;
    void setAppFolder(const juce::String&);
    juce::String vst3Folder() const;
    void setVst3Folder(const juce::String&);

private:
    UiSettings();
    std::unique_ptr<juce::PropertiesFile> file;
};
}
