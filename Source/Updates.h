#pragma once
// Update check: one small JSON file on lewonn.com lists the newest PHOQER (and KYRAX) with what's new, the
// Windows installer's link, size and SHA-256. PHOQER reads it at most once a day (HELP > CHECK EVERY DAY),
// downloads the installer only when asked, checks it against the SHA-256, and installs when the DAW (or the
// standalone app) closes. Nothing about the player is sent. PHOQER_UPDATE_URL points it at a test server.
//
// {"phoqer":{"version":"1.1.1","notes":["..."],"windows":{"url":"https://github.com/...","sha256":"...","size":123},
//            "mac":{"page":"https://lewonn.com/phoqer"}},
//  "kyrax":{"version":"1.1","notes":["..."],"page":"https://lewonn.itch.io/kyrax"}}

#include <JuceHeader.h>

namespace phoqer::updates
{
struct Release
{
    juce::String version, url, sha256, page;
    juce::StringArray notes;
    juce::int64 size = 0;
};

struct Found
{
    bool reached = false;                 // the file was read
    Release phoqer, kyrax;
    bool phoqerNewer = false, kyraxNewer = false;
};

juce::String currentVersion();                                  // this build's
bool newer(const juce::String& candidate, const juce::String& than);
juce::String manifestUrl();
// Reads the file (blocking: call it off the message thread). KYRAX counts only where it's unlocked.
Found check(bool kyraxUnlocked, const juce::String& kyraxHave = "1.0");
Found parse(const juce::var& manifest, bool kyraxUnlocked, const juce::String& kyraxHave = "1.0");
// The installer, verified: size and SHA-256 match. A file that doesn't match is deleted.
bool verify(const juce::File& installer, const Release&);
juce::File downloadFolder();
// The installer's arguments: silent, and each copy of PHOQER updated where it is. appFolder holds PHOQER.exe (/DIR),
// vst3Folder holds PHOQER.vst3 (/VST3DIR); a folder that isn't there is left out, and the installer uses its default.
juce::String installArguments(const juce::File& appFolder, const juce::File& vst3Folder);
// Runs the installer once this process has closed (the DAW, or the standalone app, which then opens again).
// Returns false where it can't (not Windows).
bool installWhenClosed(const juce::File& installer, bool reopenThisApp, const juce::File& appFolder, const juce::File& vst3Folder);
}
