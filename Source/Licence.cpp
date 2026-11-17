#include "Licence.h"
#include "ThirdParty/monocypher/monocypher-ed25519.h"
#include "ThirdParty/monocypher/monocypher.h"
#include <atomic>
#include <cstring>

#include <mutex>

namespace phoqer::licence
{
namespace
{
// The activation server's ticket key, public half. TEST pair (_build/kyrax-keys/TEST-ticket-*.txt): swap at release.
constexpr uint8_t ticketKey[32] { 0x32, 0xd5, 0xea, 0xd2, 0xfa, 0xe5, 0x15, 0x4f, 0x25, 0x1b, 0x0b, 0xf5, 0x86, 0x67, 0x94, 0x63,
                                  0x26, 0x5a, 0x49, 0xbf, 0x73, 0x86, 0x2c, 0x23, 0xa2, 0x8c, 0x44, 0x42, 0x10, 0xfb, 0xcd, 0xe2 };
constexpr const char* defaultServer = "https://keys.lewonn.com";

juce::String hashed(const juce::String& id, const char* label)
{
    return juce::SHA256((id + label).toUTF8()).toHexString().substring(0, 32);
}

// This Windows installation's own ID (empty on a Mac, or if Windows can't give it).
juce::String windowsId()
{
   #if JUCE_WINDOWS
    return juce::WindowsRegistry::getValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid", {},
                                           juce::WindowsRegistry::WoW64_64bit);
   #else
    return {};
   #endif
}

// The scrambled IDs PHOQER keeps in the unlock file; none is ever stored as it is. "machine" is the one
// unlocks always used (Windows' ID, else JUCE's device ID). Two backups let an unlock survive a change in
// one of them, such as JUCE's device ID after a JUCE update: a match on any one is the same computer, and
// PHOQER then quietly saves the current set.
juce::String machine()
{
    auto id = windowsId();
    if (id.isEmpty()) id = juce::SystemStats::getUniqueDeviceID();
    return hashed(id, "|PHOQER-KYRAX");
}
juce::String device() { return hashed(juce::SystemStats::getUniqueDeviceID(), "|PHOQER-KYRAX-DEVICE"); }
juce::String computerName() { return hashed(juce::SystemStats::getComputerName(), "|PHOQER-KYRAX-NAME"); }

juce::File unlockFile() { return folder().getChildFile("KYRAX.unlock"); }
juce::File voiceFile() { return folder().getChildFile("Voices/KYRAX.voice"); }

// KYRAX's voice from the server, stored as "PHQV1" + a 24-byte nonce + the SHA-256 of the text (so damage shows)
// + the text scrambled with XChaCha20, keyed
// by this computer's ID: a copy taken elsewhere decodes to garbage and PHOQER asks the server again.
juce::MemoryBlock voiceKey() { return juce::SHA256(("PHQVOICE1|" + machine()).toUTF8()).getRawData(); }
juce::String hashOf(const juce::String& text) { return juce::SHA256(text.toRawUTF8(), text.getNumBytesAsUTF8()).toHexString(); }
juce::File keptVoiceFile(const juce::String& sha256) { return folder().getChildFile("Voices/kept/" + sha256.substring(0, 32) + ".voice"); }
bool writeScrambled(const juce::File& file, const juce::String& text)
{
    const auto size = text.getNumBytesAsUTF8();
    uint8_t nonce[24];
    juce::Random random(juce::Random::getSystemRandom().nextInt64() ^ juce::Time::getHighResolutionTicks());
    for (auto& b : nonce) b = static_cast<uint8_t>(random.nextInt(256));
    std::vector<uint8_t> scrambled(size);
    const auto key = voiceKey();
    crypto_chacha20_x(scrambled.data(), reinterpret_cast<const uint8_t*>(text.toRawUTF8()), size, static_cast<const uint8_t*>(key.getData()), nonce, 0);
    juce::MemoryBlock out("PHQV1", 5);
    out.append(nonce, 24);
    out.append(juce::SHA256(text.toRawUTF8(), size).getRawData().getData(), 32);
    out.append(scrambled.data(), size);
    return file.getParentDirectory().createDirectory() && file.replaceWithData(out.getData(), out.getSize());
}
bool storeVoice(const juce::var& voice)
{
    const auto text = voice["data"].toString();
    const auto size = text.getNumBytesAsUTF8();
    if (size < 64 || size > 65536 || hashOf(text) != voice["sha256"].toString() || juce::JSON::parse(text)["format"].toString() != "PHQVOICE")
        return false;                                                   // damaged on the way: the next check asks again
    const auto old = serverVoiceText();                                 // the version it replaces stays for songs made with it
    if (old.isNotEmpty() && hashOf(old) != hashOf(text)) keepVoice(old);
    return writeScrambled(voiceFile(), text);
}
juce::String readScrambled(const juce::File& file)
{
    juce::MemoryBlock data;
    constexpr size_t head = 5 + 24 + 32;
    if (! file.loadFileAsData(data) || data.getSize() < head + 64 || data.getSize() > head + 65536 || std::memcmp(data.getData(), "PHQV1", 5) != 0)
        return {};
    const auto* bytes = static_cast<const uint8_t*>(data.getData());
    const size_t size = data.getSize() - head;
    std::vector<char> plain(size);
    const auto key = voiceKey();
    crypto_chacha20_x(reinterpret_cast<uint8_t*>(plain.data()), bytes + head, size, static_cast<const uint8_t*>(key.getData()), bytes + 5, 0);
    // Another computer's copy, or a damaged one: the text doesn't match the fingerprint stored with it.
    if (std::memcmp(juce::SHA256(plain.data(), size).getRawData().getData(), bytes + 29, 32) != 0) return {};
    return juce::String::fromUTF8(plain.data(), static_cast<int>(size));
}

// The ticket travels with the ID the server stamped it for ("ticketMachine"), so a quiet ID update keeps it. The
// key's name on the server ("keyId" 1-B-137), its kind, serial and gift name come from the server's answer.
bool writeRecord(const juce::String& key, const juce::String& ticket, const juce::String& ticketMachine, const juce::var& info)
{
    auto* record = new juce::DynamicObject;
    record->setProperty("key", key);
    record->setProperty("machine", machine());
    record->setProperty("device", device());
    record->setProperty("name", computerName());
    record->setProperty("source", windowsId().isNotEmpty() ? "windows" : "device");
    if (ticket.isNotEmpty()) { record->setProperty("ticket", ticket); record->setProperty("ticketMachine", ticketMachine); }
    const bool stored = info.hasProperty("keyId");                     // our own record, or the server's answer
    record->setProperty("keyId", stored ? info["keyId"] : info["id"]);
    record->setProperty("kind", info["kind"]);
    record->setProperty("serial", info["serial"]);
    record->setProperty("giftName", stored ? info["giftName"] : info["name"]);   // "name" is the scrambled computer name
    // the latest time PHOQER saw: a clock set back before it pauses KYRAX (a server answer resets it, see read())
    record->setProperty("seen", juce::jmax(static_cast<juce::int64>(info["seen"]), juce::Time::currentTimeMillis() / 1000));
    folder().createDirectory();
    return unlockFile().replaceWithText(juce::JSON::toString(juce::var(record)));
}

juce::int64 now() { return juce::Time::currentTimeMillis() / 1000; }

// "base64(payload).base64(signature)", the signature over "PHQTICKET1|" + payload. Returns when it runs
// out, or 0 when it isn't genuine or isn't for this key on this computer.
juce::int64 ticketExpiry(const juce::String& ticket, const juce::String& forKey, const juce::String& forMachine)
{
    const auto dot = ticket.indexOfChar('.');
    if (dot <= 0 || forMachine.isEmpty() || forKey.isEmpty()) return 0;
    juce::MemoryOutputStream payload, signature;
    if (! juce::Base64::convertFromBase64(payload, ticket.substring(0, dot)) || ! juce::Base64::convertFromBase64(signature, ticket.substring(dot + 1))
        || signature.getDataSize() != 64 || payload.getDataSize() > 1024)
        return 0;
    juce::MemoryBlock message("PHQTICKET1|", 11);
    message.append(payload.getData(), payload.getDataSize());
    if (crypto_ed25519_check(static_cast<const uint8_t*>(signature.getData()), ticketKey,
                             static_cast<const uint8_t*>(message.getData()), message.getSize()) != 0)
        return 0;
    const auto t = juce::JSON::parse(payload.toString());
    if (t["k"].toString() != forKey || t["m"].toString() != forMachine) return 0;
    return static_cast<juce::int64>(t["exp"]);
}

std::mutex lock;
bool loaded = false;
Status cached;
std::atomic<int> changes { 0 };
bool fileSeen = false, fileExisted = false;
juce::Time fileTime;
Transport testTransport;

// What the unlock file looks like now, so a change made by another program can be noticed.
void noteFile()
{
    const auto f = unlockFile();
    fileExisted = f.existsAsFile();
    fileTime = fileExisted ? f.getLastModificationTime() : juce::Time();
    fileSeen = true;
}

keys::Key keyOf(const juce::var& info)     // what the server said about the key
{
    keys::Key k;
    k.kind = info["kind"].toString() == "G" ? keys::Kind::gift : keys::Kind::bought;
    k.serial = static_cast<int>(info["serial"]);
    k.name = info["giftName"].toString().toStdString();
    return k;
}

Status read()
{
    Status s;
    const auto record = juce::JSON::parse(unlockFile().loadFileAsString());
    if (! keys::keyBody(record["key"].toString().toStdString())) return s;
    s.key = keyOf(record);
    // The same computer if its main ID matches, or its device ID, or (only where the main ID was never
    // Windows' own) its name. Unlock files from before these backups have only "machine".
    const auto storedDevice = record["device"].toString(), storedName = record["name"].toString();
    const bool sameMachine = record["machine"].toString() == machine();
    const bool same = sameMachine
                   || (storedDevice.isNotEmpty() && storedDevice == device())
                   || (record["source"].toString() == "device" && windowsId().isEmpty() && storedName.isNotEmpty() && storedName == computerName());
    if (same && (! sameMachine || storedDevice != device() || storedName != computerName()))
        writeRecord(record["key"].toString(), record["ticket"].toString(), record["ticketMachine"].toString(), record);     // quietly bring the IDs up to date
    if (! same) { s.lockedElsewhere = true; return s; }
    // This computer's key: it plays while the server's ticket lasts. No ticket (or a run-out one) waits for PHOQER to be online once.
    // The pass counts only if it was stamped for this computer as it is now: the IDs in the file can be edited, so a
    // copied file whose IDs were changed to match another computer is refused. (After a real ID change, the next
    // time PHOQER is online it gets a fresh pass, as it does for a pass that ran out.)
    const auto stamped = record["ticketMachine"].toString();
    auto expiry = stamped == machine() ? ticketExpiry(record["ticket"].toString(), record["keyId"].toString(), stamped) : 0;
    // a clock set back more than an hour before the latest time seen here doesn't stretch the 14 days
    const auto seen = static_cast<juce::int64>(record["seen"]);
    if (expiry > 0 && now() + 3600 < seen) expiry = 0;
    else if (expiry > 0 && now() > seen + 3600) writeRecord(record["key"].toString(), record["ticket"].toString(), stamped, record);
    const auto left = expiry - now();
    s.unlocked = left > 0;
    s.paused = ! s.unlocked;
    s.daysLeft = s.unlocked ? static_cast<int>((left + 86399) / 86400) : 0;
    s.ticketRanOut = s.paused && expiry > 0;
    return s;
}

juce::var call(const juce::String& path, const juce::var& body)
{
    if (testTransport) return testTransport(path, body);
    auto server = juce::SystemStats::getEnvironmentVariable("PHOQER_ACTIVATION_URL", defaultServer).trimCharactersAtEnd("/");
    int status = 0;
    std::unique_ptr<juce::InputStream> stream;
    // Windows can hand a request an old connection the server already closed; one retry gets a fresh one.
    for (int attempt = 0; attempt < 2 && stream == nullptr; ++attempt)
    {
        if (attempt > 0) juce::Thread::sleep(400);
        stream = juce::URL(server + path).withPOSTData(juce::JSON::toString(body, true))
                     .createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withExtraHeaders("Content-Type: application/json")
                                            .withConnectionTimeoutMs(8000)
                                            .withStatusCode(&status));
    }
    if (stream == nullptr) return {};
    const auto reply = juce::JSON::parse(stream->readEntireStreamAsString());
    return reply.isObject() ? reply : juce::var();
}

juce::var activationRequest(const juce::String& key)
{
    auto* body = new juce::DynamicObject;
    body->setProperty("key", key);
    body->setProperty("machine", machine());
   #if JUCE_MAC
    body->setProperty("os", "Mac");
   #else
    body->setProperty("os", "Windows");
   #endif
    body->setProperty("phoqer", JucePlugin_VersionString);              // the server sends KYRAX's voice only to a PHOQER that can play it
    const auto voice = serverVoiceText();                               // and only when this computer's copy is missing or not the current one
    body->setProperty("voice", voice.isEmpty() ? juce::String() : hashOf(voice));
    return juce::var(body);
}

void forget()     // after a change made here: the next status() reads the file again
{
    const std::lock_guard<std::mutex> guard(lock);
    loaded = false;
    ++changes;
}

// Every running PHOQER shares one worker thread for the licence.
struct Background
{
    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withThreadName("PHOQER licence").withNumberOfThreads(1) };
    std::atomic<bool> renewedThisRun { false };
    // Closing waits for queued work to finish (a key check or renewal takes at most ~8 s), so none is dropped half done.
    ~Background()
    {
        for (int waited = 0; pool.getNumJobs() > 0 && waited < 10000; waited += 10) juce::Thread::sleep(10);
    }
};
std::mutex backgroundLock;
std::weak_ptr<Background> currentBackground;
}

Online::Online()
{
    const std::lock_guard<std::mutex> guard(backgroundLock);
    auto b = currentBackground.lock();
    if (b == nullptr) { b = std::make_shared<Background>(); currentBackground = b; }
    shared = b;
}
Online::~Online() = default;
void Online::run(std::function<void()> job) { static_cast<Background*>(shared.get())->pool.addJob(std::move(job)); }

juce::String keyId(const keys::Key& k)
{
    return juce::String(k.product) + (k.kind == keys::Kind::gift ? "-G-" : "-B-") + juce::String(k.serial);
}

void setTransportForTests(Transport t) { testTransport = std::move(t); }

juce::File folder()
{
    const auto overridden = juce::SystemStats::getEnvironmentVariable("PHOQER_LICENCE_DIR", {});
    if (overridden.isNotEmpty()) return juce::File(overridden);
   #if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support/PHOQER");
   #else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("PHOQER");
   #endif
}

Status status()
{
    const std::lock_guard<std::mutex> guard(lock);
    if (! loaded) { cached = read(); loaded = true; noteFile(); }
    return cached;
}

juce::String enter(const juce::String& text, int* used)
{
    if (! keys::keyBody(text.toStdString())) return "THAT IS NOT A VALID KYRAX KEY.";
    const auto reply = call("/activate", activationRequest(text.trim()));
    if (! reply.isObject()) return "PHOQER NEEDS THE INTERNET ONCE TO UNLOCK KYRAX.";
    if (! static_cast<bool>(reply["ok"]))
    {
        const auto code = reply["code"].toString();
        return code == "full" ? "THIS KEY IS ON 3 COMPUTERS ALREADY."
             : code == "banned" ? "THIS KEY HAS BEEN BLOCKED."
             : code == "invalid" ? "THAT IS NOT A VALID KYRAX KEY."
             : code == "slow" ? "TOO MANY WRONG KEYS. TRY AGAIN IN AN HOUR."
                                 : "THE ACTIVATION SERVER SAID NO.";
    }
    const auto ticket = reply["ticket"].toString();
    if (ticketExpiry(ticket, reply["id"].toString(), machine()) <= now()) return "THE ACTIVATION SERVER'S ANSWER WAS NOT GENUINE.";
    if (! writeRecord(text.trim(), ticket, machine(), reply)) return "COULD NOT SAVE THE UNLOCK.";
    if (reply["voice"].isObject()) storeVoice(reply["voice"]);
    if (used != nullptr) *used = static_cast<int>(reply["used"]);
    {
        const std::lock_guard<std::mutex> guard(lock);
        cached = read();
        loaded = true;
        noteFile();
        ++changes;
    }
    return {};
}

void renewInBackground(Online& online)
{
    auto* background = static_cast<Background*>(online.shared.get());
    if (background->renewedThisRun.exchange(true)) return;
    background->pool.addJob([]
    {
        const auto record = juce::JSON::parse(unlockFile().loadFileAsString());
        const auto s = status();
        if (! keys::keyBody(record["key"].toString().toStdString()) || s.lockedElsewhere || (! s.unlocked && ! s.paused)) return;
        // A fresh ticket covers 14 days: renew once it's a day old, or at once while KYRAX's voice is missing here.
        const auto expiry = ticketExpiry(record["ticket"].toString(), record["keyId"].toString(), record["ticketMachine"].toString());
        if (expiry - now() > (offlineDays - 1) * 86400 && serverVoiceText().isNotEmpty()) return;
        const auto reply = call("/activate", activationRequest(record["key"].toString()));
        if (! reply.isObject()) return;                                 // offline: the ticket keeps counting down
        if (static_cast<bool>(reply["ok"]) && ticketExpiry(reply["ticket"].toString(), reply["id"].toString(), machine()) > now())
        {
            writeRecord(record["key"].toString(), reply["ticket"].toString(), machine(), reply);
            if (reply["voice"].isObject()) storeVoice(reply["voice"]);     // a new KYRAX voice: used from PHOQER's next start
        }
        else if (reply["code"].toString() == "banned")
            writeRecord(record["key"].toString(), {}, {}, record);      // blocked: the ticket goes, KYRAX pauses
        forget();
    });
}

void deactivate()
{
    const auto record = juce::JSON::parse(unlockFile().loadFileAsString());
    remove();                                                           // off this computer at once
    folder().getChildFile("Voices/kept").deleteRecursively();           // with every KYRAX version it kept for songs
    if (keys::keyBody(record["key"].toString().toStdString()))
    {
        auto* body = new juce::DynamicObject;
        body->setProperty("key", record["key"]);
        const auto stamped = record["ticketMachine"].toString();
        body->setProperty("machine", stamped.isNotEmpty() ? stamped : machine());
        call("/deactivate", juce::var(body));                           // offline: the slot stays taken until the owner frees it
    }
}

void remove()
{
    unlockFile().deleteFile();
    voiceFile().deleteFile();
    const std::lock_guard<std::mutex> guard(lock);
    cached = {};
    loaded = true;
    noteFile();
    ++changes;
}

void reload()
{
    const std::lock_guard<std::mutex> guard(lock);
    loaded = false;
    ++changes;
}

void checkForChanges()
{
    const auto f = unlockFile();
    const bool exists = f.existsAsFile();
    const auto time = exists ? f.getLastModificationTime() : juce::Time();
    const std::lock_guard<std::mutex> guard(lock);
    if (fileSeen && exists == fileExisted && time == fileTime) return;
    fileSeen = true;
    fileExisted = exists;
    fileTime = time;
    loaded = false;
    ++changes;
}

int generation() { return changes.load(); }

juce::String serverVoiceText() { return readScrambled(voiceFile()); }

void keepVoice(const juce::String& text)
{
    const auto size = text.getNumBytesAsUTF8();
    if (size < 64 || size > 65536) return;
    const auto file = keptVoiceFile(hashOf(text));
    if (! file.existsAsFile()) writeScrambled(file, text);
}

juce::String keptVoice(const juce::String& sha256)
{
    if (sha256.length() != 64) return {};
    const auto current = serverVoiceText();
    if (current.isNotEmpty() && hashOf(current) == sha256) return current;
    const auto kept = readScrambled(keptVoiceFile(sha256));
    return kept.isNotEmpty() && hashOf(kept) == sha256 ? kept : juce::String();
}

juce::String fetchVoice(const juce::String& sha256)
{
    // Already here (another KYRAX track in the same song fetched it a moment ago): no server call, so a song with many
    // KYRAX tracks asks the server once, well inside its rate limit.
    if (const auto here = keptVoice(sha256); here.isNotEmpty()) return here;
    const auto record = juce::JSON::parse(unlockFile().loadFileAsString());
    if (sha256.length() != 64 || ! keys::keyBody(record["key"].toString().toStdString())) return {};
    auto* body = new juce::DynamicObject;
    body->setProperty("key", record["key"]);
    const auto stamped = record["ticketMachine"].toString();
    body->setProperty("machine", stamped.isNotEmpty() ? stamped : machine());
    body->setProperty("sha256", sha256);
    const auto reply = call("/voice", juce::var(body));
    const auto text = reply["voice"]["data"].toString();
    if (! static_cast<bool>(reply["ok"]) || hashOf(text) != sha256 || juce::JSON::parse(text)["format"].toString() != "PHQVOICE") return {};
    keepVoice(text);
    return text;
}

juce::String describe(const Status& s)
{
    if (! s.unlocked) return s.paused ? "PAUSED - GO ONLINE ONCE" : s.lockedElsewhere ? "LOCKED ON THIS COMPUTER" : "LOCKED - NEEDS A KEY";
    if (s.key.kind == keys::Kind::gift)
        return "A GIFT FROM LEWONN TO " + juce::String(s.key.name).toUpperCase();
    return "LICENSED - KEY #" + juce::String(s.key.serial).paddedLeft('0', 4);
}
}
