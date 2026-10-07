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

private:
    UiSettings();
    std::unique_ptr<juce::PropertiesFile> file;
};
}
