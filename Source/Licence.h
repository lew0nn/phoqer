#pragma once
// Which paid voices this computer has unlocked. A key is checked offline against the public key in
// Licence.cpp, then once online with lewonn.com's activation server: a key works on up to 3 computers,
// and the server stamps a ticket that lets KYRAX play offline for 14 days. Each time PHOQER is online
// (at most once a day) the ticket is renewed. The unlock and ticket live in a small file tied to this
// computer, so copied settings ask for the key again.

#include <JuceHeader.h>
#include "LicenceKey.h"

namespace phoqer::licence
{
inline constexpr int offlineDays = 14;

struct Status
{
    bool unlocked = false;
    bool lockedElsewhere = false;   // an unlock exists, but it was made on another computer
    bool paused = false;            // this computer's unlock, but its ticket ran out: go online once
    int daysLeft = -1;              // whole days the ticket still covers (-1: no unlock)
    bool ticketRanOut = false;      // paused because 14 days passed offline (not a first check still on its way)
    keys::Key key;
};

Status status();                               // KYRAX; cached for the process, read once
// Checks the key, asks the activation server (blocking: call it off the message thread) and saves the
// unlock with its ticket. Empty when unlocked, else the first line to tell the player. `used` gets the
// number of computers this key is on now.
juce::String enter(const juce::String& text, int* used = nullptr);
// The licence's background work (key checks, renewals). Each PHOQER owns one; the last to close waits
// for any running job, so no thread outlives an unloaded plugin.
class Online final
{
public:
    Online();
    ~Online();
    void run(std::function<void()> job);
private:
    std::shared_ptr<void> shared;
    friend void renewInBackground(Online&);
};
void renewInBackground(Online&);               // quietly renews the ticket when it's a day old and PHOQER is online (once per run)
void deactivate();                             // REMOVE KEY: frees this computer's slot on the server (best effort), then remove()
void remove();
void reload();                                 // forget the cached state; read the unlock again
void checkForChanges();                        // re-read the unlock if its file changed (another program entered or removed a key)
int generation();                              // goes up each time the unlock may have changed
juce::String describe(const Status&);          // "A GIFT FROM LEWONN TO BOB", "LICENSED - KEY #0137"
juce::String keyId(const keys::Key&);          // the server's name for a key: "1-B-137", "1-G-4"
juce::File folder();
// KYRAX's voice as the activation server sent it to this computer, or empty. It is kept scrambled with a key
// made from this computer's ID, so a copy taken to another computer doesn't open there.
juce::String serverVoiceText();

// The activation server's answer to POST path + JSON body; a void var when it can't be reached.
// Checks replace it with a stand-in; PHOQER_ACTIVATION_URL points the real one elsewhere (a local test server).
using Transport = std::function<juce::var(const juce::String& path, const juce::var& body)>;
void setTransportForTests(Transport);
}
