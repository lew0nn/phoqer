#pragma once

#include <JuceHeader.h>
#include "VoiceData.h"
#include "Licence.h"
#include <atomic>
#include <map>

// The paid voice (KYRAX) for one plugin instance: its file, whether this computer has unlocked it, which
// voice is chosen, the snapshot songs and presets keep (so an update never changes a saved sound), the
// one-time notices, and the undo tokens. KYRAX ships with PHOQER as a separate data file and plays only
// once this computer has unlocked it with a key.
//
// Everything runs on the message thread except audioVoice(), which the audio thread calls once a block.
class VoiceSelection final
{
public:
    VoiceSelection(juce::AudioProcessor& host, juce::AudioProcessorValueTreeState& state);

    // Audio thread: picks up a newly loaded voice without waiting for a lock, and returns the voice to
    // play, or nullptr when a free seal plays.
    const phoqer::CharacterPreset* audioVoice() noexcept;

    bool loadVoiceFile(const juce::File&, juce::String& error);   // other voice data (the checks)
    void useLoadedVoice();
    void selectFreeVoice(int index);
    int getVisualVoice() const noexcept { return loadedSelected.load() ? 3 : juce::jlimit(0, 2, static_cast<int>(std::lround(character.load()))); }
    bool hasLoadedVoice() const noexcept { return voiceAvailable.load() && voiceUnlocked.load(); }
    bool isVoiceInstalled() const noexcept { return voiceAvailable.load(); }
    // LOAD VOICE / a dropped file: checks it is KYRAX, copies it into PHOQER's own Voices folder, plays it.
    bool installVoiceFile(const juce::File&, juce::String& error);
    static juce::File installedVoiceFile();
    bool isVoiceUnlocked() const noexcept { return voiceUnlocked.load(); }
    void refreshLicence();                       // after a key is entered or removed
    phoqer::licence::Online& online() noexcept { return licenceOnline; }   // key checks run here, off the message thread
    juce::String loadedVoiceName() const;
    juce::String loadedVoiceHash() const;
    juce::String takeVoiceNotice();              // once: why a song's voice could not play
    juce::String captureVoiceSelection() const;
    void restoreVoiceSelection(const juce::String&);
    juce::String captureVoiceToken() const;      // the same, as a short token for the undo history
    void restoreVoiceToken(const juce::String&);
    void notifyVoiceChange();                    // tell the host the voice changed (it is not a parameter)
    static juce::File bundledVoiceFile();
    static phoqer::VoiceData bundledVoice();

private:
    void setVoiceData(phoqer::VoiceData);

    juce::AudioProcessor& host;
    juce::AudioProcessorValueTreeState& state;
    std::atomic<float>& character;
    mutable juce::SpinLock voiceLock;
    phoqer::VoiceData loadedData;
    phoqer::CharacterPreset playing;             // the audio thread's copy of the loaded voice
    bool voicePending = false;
    std::atomic<bool> loadedSelected { false }, voiceAvailable { false }, voiceUnlocked { false };
    juce::String loadNotice;
    // Once a second: a key entered or removed in another instance or program reaches this one.
    struct LicenceWatch final : juce::Timer
    {
        explicit LicenceWatch(VoiceSelection& v) : owner(v) { startTimer(1000); }
        void timerCallback() override;
        VoiceSelection& owner;
        int seen = -1;
    };
    phoqer::licence::Online licenceOnline;
    LicenceWatch licenceWatch { *this };
    mutable std::map<juce::String, juce::String> voiceTokens;     // undo tokens and the selections they stand for
    juce::String keptSelection;     // a song's locked voice, saved again unchanged until another voice is chosen

    JUCE_DECLARE_NON_COPYABLE(VoiceSelection)
};
