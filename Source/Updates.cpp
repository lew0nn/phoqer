#include "Updates.h"

#if JUCE_WINDOWS
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentProcessId(void);
#endif

namespace phoqer::updates
{
juce::String currentVersion() { return JucePlugin_VersionString; }

bool newer(const juce::String& candidate, const juce::String& than)
{
    juce::StringArray a, b;
    a.addTokens(candidate.trim(), ".", {});
    b.addTokens(than.trim(), ".", {});
    for (int i = 0; i < juce::jmax(a.size(), b.size()); ++i)
    {
        const int x = a[i].getIntValue(), y = b[i].getIntValue();
        if (x != y) return x > y;
    }
    return false;
}

juce::String manifestUrl()
{
    return juce::SystemStats::getEnvironmentVariable("PHOQER_UPDATE_URL", "https://lewonn.com/updates.json");
}

Found parse(const juce::var& m, bool kyraxUnlocked, const juce::String& kyraxHave)
{
    Found f;
    if (! m.isObject()) return f;
    f.reached = true;
    const auto notes = [](const juce::var& list)
    {
        juce::StringArray out;
        if (auto* a = list.getArray())
            for (const auto& line : *a)
                if (out.size() < 3) out.add(line.toString().toUpperCase().substring(0, 34));
        return out;
    };
    const auto p = m["phoqer"];
    f.phoqer.version = p["version"].toString();
    f.phoqer.notes = notes(p["notes"]);
   #if JUCE_WINDOWS
    f.phoqer.url = p["windows"]["url"].toString();
    f.phoqer.sha256 = p["windows"]["sha256"].toString().toLowerCase();
    f.phoqer.size = static_cast<juce::int64>(p["windows"]["size"]);
   #endif
    f.phoqer.page = p["mac"]["page"].toString();
    // Windows needs a download it can check; a Mac only needs the page.
   #if JUCE_WINDOWS
    // Only from lewonn's own GitHub releases (or the local test server): a changed update file can't point elsewhere.
    const bool usable = f.phoqer.url.startsWith("https://github.com/lew0nn/") || f.phoqer.url.startsWith("http://127.0.0.1:");
    f.phoqerNewer = newer(f.phoqer.version, currentVersion()) && usable && f.phoqer.sha256.length() == 64 && f.phoqer.size > 0;
   #else
    f.phoqerNewer = newer(f.phoqer.version, currentVersion()) && f.phoqer.page.startsWithIgnoreCase("https://");
   #endif
    const auto k = m["kyrax"];
    f.kyrax.version = k["version"].toString();
    f.kyrax.notes = notes(k["notes"]);
    f.kyrax.page = k["page"].toString();
    f.kyraxNewer = kyraxUnlocked && newer(f.kyrax.version, kyraxHave) && f.kyrax.page.startsWithIgnoreCase("https://");
    return f;
}

Found check(bool kyraxUnlocked, const juce::String& kyraxHave)
{
    auto stream = juce::URL(manifestUrl()).createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                                 .withConnectionTimeoutMs(8000)
                                                                 .withExtraHeaders("Cache-Control: no-cache"));
    if (stream == nullptr) return {};
    return parse(juce::JSON::parse(stream->readEntireStreamAsString()), kyraxUnlocked, kyraxHave);
}

juce::File downloadFolder()
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("PHOQER-update");
}

bool verify(const juce::File& installer, const Release& r)
{
    const bool ok = installer.existsAsFile() && installer.getSize() == r.size
                 && juce::SHA256(installer).toHexString().toLowerCase() == r.sha256;
    if (! ok) installer.deleteFile();
    return ok;
}

juce::String installArguments(const juce::File& appFolder, const juce::File& vst3Folder)
{
    // PowerShell single-quoted items: a ' inside a path is doubled
    const auto item = [](const juce::String& name, const juce::File& f) { return ",'/" + name + "=\"" + f.getFullPathName().replace("'", "''") + "\"'"; };
    juce::String args = "'/SP-','/SILENT'";
    if (appFolder != juce::File() && appFolder.isDirectory()) args << item("DIR", appFolder);
    if (vst3Folder != juce::File() && vst3Folder.getChildFile("PHOQER.vst3").isDirectory()) args << item("VST3DIR", vst3Folder);
    return args;
}

bool installWhenClosed(const juce::File& installer, bool reopenThisApp, const juce::File& appFolder, const juce::File& vst3Folder)
{
   #if JUCE_WINDOWS
    const auto quote = [](const juce::String& s) { return "'" + s.replace("'", "''") + "'"; };
    // A hidden PowerShell waits for this process to close, then runs the installer (and opens PHOQER again).
    const auto app = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::String script = "Wait-Process -Id " + juce::String(static_cast<juce::int64>(GetCurrentProcessId())) + " -ErrorAction SilentlyContinue; "
                          "Start-Process -Wait -FilePath " + quote(installer.getFullPathName()) + " -ArgumentList " + installArguments(appFolder, vst3Folder);
    if (reopenThisApp)
        script << "; Start-Process -FilePath " << quote(app.getFullPathName());
    const auto powershell = juce::File::getSpecialLocation(juce::File::windowsSystemDirectory).getChildFile("WindowsPowerShell\\v1.0\\powershell.exe");
    return powershell.startAsProcess("-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -Command \"" + script.replace("\"", "\\\"") + "\"");
   #else
    juce::ignoreUnused(installer, reopenThisApp, appFolder, vst3Folder);
    return false;
   #endif
}
}
