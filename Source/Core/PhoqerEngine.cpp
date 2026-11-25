#include "PhoqerEngine.h"
#include "DspPrimitives.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace phoqer
{
PhoqerEngine::PhoqerEngine()
    : voices(makeVoices(ageCounter, std::make_index_sequence<voiceSlots> {}))
{
    pitchWheels.fill(8192);
}

void PhoqerEngine::prepare(double newSampleRate, int maximumBlockSize, int)
{
    sampleRate = newSampleRate;
    for (auto& voice : voices)
        voice.prepare(sampleRate, maximumBlockSize);

    chorusStage.prepare(sampleRate);
    spaceStage.prepare(sampleRate, maximumBlockSize);
    outputStage.prepare(sampleRate);
    fishSong.prepare(sampleRate);
    telemetry.setSampleRate(sampleRate);
    reset();
}

void PhoqerEngine::reset()
{
    fishSong.reset();
    silentSamples = 0;
    for (auto& voice : voices)
        voice.hardReset();
    pitchWheels.fill(8192);
    pending = {};
    arpKeys = {};
    arpKeyCount = 0; arpOrder = 0; arpStep = 0; arpGrid = 0.0;
    arpLastBeat = -1.0e9; arpNextBeat = 0.0; arpWasSynced = false; arpLastStepAt = -1; arpNextStep = 0; arpLane = -1;
    arpNextAt = arpOffAt = -1; arpSounding = -1; arpWasActive = false;
    yapBeats = 0.0;
    sustainDown.fill(false);
    sustained.fill(false);
    modWheel = 0.0f;
    newestVoice = nullptr;
    heldKeyCount = 0;
    chorusStage.reset();
    spaceStage.reset();
    outputStage.reset();
    activeCharacter = defaultSealCharacter;
    telemetry.publishFace({});
    telemetry.publishMeter(0.0f, 0.0f);
}

// Eight notes sound at once. A ninth takes over the least important note: that one fades out over 3 ms in
// its own slot while the new note starts in the spare one. Only if every slot is busy (a fade still running)
// is a voice cut, the quietest fading one, with its short 32-sample tail.
SealVoice* PhoqerEngine::findVoiceForNoteOn(int usableSlots, int playingLimit) noexcept
{
    int playing = 0;
    SealVoice* freeVoice = nullptr;
    SealVoice* quietestFading = nullptr;
    for (int slot = 0; slot < usableSlots; ++slot)
    {
        auto& voice = voices[static_cast<size_t>(slot)];
        if (! voice.isActive()) { if (freeVoice == nullptr) freeVoice = &voice; }
        else if (voice.isFading())
        {
            if (quietestFading == nullptr || voice.getEnvelopeLevel() < quietestFading->getEnvelopeLevel())
                quietestFading = &voice;
        }
        else ++playing;
    }
    if (playing >= playingLimit)
        if (auto* victim = leastImportantVoice(usableSlots))
            victim->fadeOut();
    return freeVoice != nullptr ? freeVoice : quietestFading;
}

// The note to give up first: the quietest released one, else the quietest held one, with a little
// protection for notes just played.
SealVoice* PhoqerEngine::leastImportantVoice(int usableSlots) noexcept
{
    SealVoice* bestReleased = nullptr;
    SealVoice* bestActive = nullptr;
    float releasedLevel = std::numeric_limits<float>::max();
    float activePriority = std::numeric_limits<float>::max();
    uint64_t newestAge = 0;

    for (const auto& voice : voices)
        newestAge = std::max(newestAge, voice.getAge());

    for (int slot = 0; slot < usableSlots; ++slot)
    {
        auto& voice = voices[static_cast<size_t>(slot)];
        if (! voice.isActive() || voice.isFading())
            continue;
        const auto level = voice.getEnvelopeLevel();
        if (voice.isReleasing())
        {
            if (level < releasedLevel || (level == releasedLevel
                                          && (bestReleased == nullptr || voice.getAge() < bestReleased->getAge())))
            {
                bestReleased = &voice;
                releasedLevel = level;
            }
            continue;
        }

        const auto recencyProtection = 0.04f / (1.0f + static_cast<float>(newestAge - voice.getAge()));
        const auto priority = level + recencyProtection;
        if (priority < activePriority || (priority == activePriority
                                          && (bestActive == nullptr || voice.getAge() < bestActive->getAge())))
        {
            bestActive = &voice;
            activePriority = priority;
        }
    }

    return bestReleased != nullptr ? bestReleased : bestActive;
}

float PhoqerEngine::getNewestPitchNote() const noexcept
{
    return newestVoice != nullptr ? newestVoice->getPitchNote() : -1.0f;
}

int PhoqerEngine::getActiveVoiceCount() const noexcept
{
    int count = 0;
    for (const auto& voice : voices) if (voice.isActive()) ++count;
    return count;
}

void PhoqerEngine::handleEvent(const MidiEvent& event, int sampleInBlock) noexcept
{
    const auto channel = clamp(0, 15, event.channel);
    switch (event.type)
    {
        case MidiEventType::noteOn:
        {
            if (event.value <= 0.0f)
            {
                releaseNote(channel, event.note);
                break;
            }

            telemetry.beginCall();
            const bool nitro = nitroActive();
            keyDown(channel, clamp(0, 127, event.note));
            if (macros.mono)
            {
                // shortcut: NITRO's extra notes don't follow a legato move, so with NITRO on each key starts afresh.
                if (auto* held = monoNote(); held != nullptr && ! nitro)
                {
                    held->legatoTo(channel, clamp(0, 127, event.note));
                    sustained[static_cast<size_t>(held - voices.data())] = false;     // the new key holds it now, not the pedal
                    break;
                }
                for (auto& p : pending) p.used = false;
                for (size_t i = 0; i < voices.size(); ++i)     // one note at a time: the rest fade out (3 ms)
                    if (voices[i].isActive() && ! voices[i].isFading()) { voices[i].fadeOut(); sustained[i] = false; }
            }
            int extras = 0;
            float groupPower = 1.0f;
            if (nitro)
                for (int k = 0; k < nitroNotes; ++k)
                    if (const int off = macros.nitro.offsets[static_cast<size_t>(k)]; off != 0 && event.note + off >= 0 && event.note + off <= 127)
                    {
                        const float level = clamp(0.0f, 1.0f, macros.nitro.levels[static_cast<size_t>(k)]);
                        ++extras;
                        groupPower += level * level;
                    }
            if (auto* voice = nitro ? nitroVoiceFor() : findVoiceForNoteOn(plainSlots, voiceCount))
            {
                sustained[static_cast<size_t>(voice - voices.data())] = false;
                if (voice->isActive())
                    voice->stopNote(false);
                // GLIDE: a note played while the previous one is still held slides in from its pitch.
                // Notes struck within 30 ms of each other are a chord and start on their own pitches.
                constexpr float chordWindowSeconds = 0.030f;
                const bool overlapping = newestVoice != nullptr && newestVoice != voice
                                      && newestVoice->isActive() && ! newestVoice->isReleasing()
                                      && newestVoice->getNoteAge() >= chordWindowSeconds;
                const float from = overlapping ? newestVoice->getPitchNote() : -1.0f;
                voice->startNote(channel, clamp(0, 127, event.note),
                                 clamp(0.0f, 1.0f, event.value), pitchWheels[static_cast<size_t>(channel)]);
                if (from >= 0.0f) voice->glideFrom(from);
                newestVoice = voice;
                if (extras > 0)
                {
                    voice->setLevel(1.0f / std::sqrt(groupPower));     // the whole group as loud as the key alone
                    queueNitro(channel, clamp(0, 127, event.note), clamp(0.0f, 1.0f, event.value), sampleInBlock);
                }
            }
            break;
        }
        case MidiEventType::noteOff:
            releaseNote(channel, event.note);
            break;
        case MidiEventType::treat:
            fishSong.start(0.05, ++fishSongs);     // a different note each time
            break;
        case MidiEventType::belch:
            telemetry.beginCall();
            if (auto* voice = findVoiceForNoteOn(plainSlots, voiceCount))
            {
                sustained[static_cast<size_t>(voice - voices.data())] = false;
                if (voice->isActive())
                    voice->stopNote(false);
                voice->startBelch(clamp(12, 127, event.note), clamp(0.0f, 1.0f, event.value));
            }
            break;
        case MidiEventType::pitchWheel:
        {
            const auto value = clamp(-1.0f, 1.0f, event.value);
            const auto wheel = clamp(0, 16383, static_cast<int>(8192.0f + value * 8192.0f));
            pitchWheels[static_cast<size_t>(channel)] = wheel;
            for (auto& voice : voices)
                if (voice.matchesChannel(channel))
                    voice.pitchWheelMoved(wheel);
            break;
        }
        case MidiEventType::allNotesOff:
            sustainDown[static_cast<size_t>(channel)] = false;
            heldKeyCount = 0;
            for (auto& p : pending) if (p.channel == channel) p.used = false;
            for (size_t i = 0; i < voices.size(); ++i)
                if (voices[i].matchesChannel(channel))
                {
                    voices[i].stopNote(true);
                    sustained[i] = false;
                }
            fishSong.reset();     // Panic (and a host's all-notes-off) also silences the fish song
            break;
        case MidiEventType::sustainPedal:
            sustainDown[static_cast<size_t>(channel)] = event.value >= 0.5f;
            if (! sustainDown[static_cast<size_t>(channel)])      // pedal up: let go of what it was holding
            {
                for (auto& p : pending)
                    if (p.used && p.channel == channel && p.sustained) { p.sustained = false; p.released = true; }
                for (size_t i = 0; i < voices.size(); ++i)
                    if (sustained[i] && voices[i].matchesChannel(channel))
                    {
                        voices[i].stopNote(true);
                        sustained[i] = false;
                    }
            }
            break;
        case MidiEventType::modWheel:
            modWheel = clamp(0.0f, 1.0f, event.value);
            break;
    }
}

// A key comes up: the voice is released, unless the sustain pedal is down, which holds it.
void PhoqerEngine::releaseNote(int channel, int note) noexcept
{
    const bool pedal = sustainDown[static_cast<size_t>(channel)];
    // MONO: letting go of the sounding key while others are still down moves the note back to the newest of them.
    if (keyUp(channel, note) && macros.mono && ! pedal && heldKeyCount > 0)
        if (auto* held = monoNote(); held != nullptr && held->matchesNote(channel, note))
        {
            const auto back = heldKeys[static_cast<size_t>(heldKeyCount - 1)];
            held->legatoTo(back.channel, back.note);
            return;
        }
    for (auto& p : pending)     // NITRO notes still waiting for their strum start: they end as soon as they start
        if (p.used && p.channel == channel && p.root == note && ! p.released && ! p.sustained)
            (pedal ? p.sustained : p.released) = true;
    for (size_t i = 0; i < voices.size(); ++i)
        if (voices[i].matchesNote(channel, note) || voices[i].matchesLayerOf(channel, note))
        {
            if (sustainDown[static_cast<size_t>(channel)]) sustained[i] = true;
            else voices[i].stopNote(true);
        }
}

void PhoqerEngine::keyDown(int channel, int note) noexcept
{
    keyUp(channel, note);
    if (heldKeyCount == static_cast<int>(heldKeys.size()))     // full: the oldest key drops out of the list
    {
        std::move(heldKeys.begin() + 1, heldKeys.end(), heldKeys.begin());
        --heldKeyCount;
    }
    heldKeys[static_cast<size_t>(heldKeyCount++)] = { channel, note };
}

bool PhoqerEngine::keyUp(int channel, int note) noexcept
{
    for (int i = 0; i < heldKeyCount; ++i)
        if (heldKeys[static_cast<size_t>(i)].channel == channel && heldKeys[static_cast<size_t>(i)].note == note)
        {
            std::move(heldKeys.begin() + i + 1, heldKeys.begin() + heldKeyCount, heldKeys.begin() + i);
            --heldKeyCount;
            return true;
        }
    return false;
}

SealVoice* PhoqerEngine::monoNote() noexcept
{
    if (newestVoice == nullptr) return nullptr;
    auto& voice = voices[static_cast<size_t>(newestVoice - voices.data())];
    return voice.isActive() && ! voice.isReleasing() && ! voice.isFading() && ! voice.isExtra() ? &voice : nullptr;
}

void PhoqerEngine::renderVoices(AudioBuffer& output, int startSample, int numSamples)
{
    if (numSamples <= 0)
        return;
    for (auto& voice : voices)
        voice.renderNextBlock(output, startSample, numSamples);
}

// A voice for a NITRO note. At the limit, the oldest extra note (a released one first) fades out to make room.
SealVoice* PhoqerEngine::nitroVoiceFor() noexcept
{
    int playing = 0;
    SealVoice* oldest = nullptr;
    for (auto& voice : voices)
    {
        if (! voice.isActive() || voice.isFading()) continue;
        ++playing;
        if (voice.isExtra() && (oldest == nullptr || voice.isReleasing() > oldest->isReleasing()
                                || (voice.isReleasing() == oldest->isReleasing() && voice.getAge() < oldest->getAge())))
            oldest = &voice;
    }
    if (playing >= nitroVoiceLimit && oldest != nullptr) oldest->fadeOut();
    return findVoiceForNoteOn(voiceSlots, nitroVoiceLimit);
}

bool PhoqerEngine::nitroActive() const noexcept
{
    if (! macros.nitro.on || macros.loadedVoice == nullptr) return false;
    for (const auto off : macros.nitro.offsets) if (off != 0) return true;
    return false;
}

// NITRO: the key's extra notes, spread across the width, detuned sharp and flat in turn, each started
// strumMs after the one before. The group is level-matched to the key alone.
void PhoqerEngine::queueNitro(int channel, int note, float velocity, int sampleInBlock) noexcept
{
    std::array<int, nitroNotes> slots {};
    int n = 0;
    float power = 1.0f;
    for (int k = 0; k < nitroNotes; ++k)
        if (const int off = macros.nitro.offsets[static_cast<size_t>(k)]; off != 0 && note + off >= 0 && note + off <= 127)
        {
            const float level = clamp(0.0f, 1.0f, macros.nitro.levels[static_cast<size_t>(k)]);
            slots[static_cast<size_t>(n++)] = k;
            power += level * level;
        }
    const float groupGain = 1.0f / std::sqrt(power);
    const float width = clamp(0.0f, 1.0f, macros.nitro.width);
    const double strum = clamp(0.0f, 100.0f, macros.nitro.strumMs) * 0.001 * sampleRate;
    for (int i = 0; i < n; ++i)
    {
        const int k = slots[static_cast<size_t>(i)];
        PendingNote* slot = nullptr;
        for (auto& p : pending) if (! p.used) { slot = &p; break; }
        if (slot == nullptr) return;     // ponytail: 64 waiting notes at most; a denser burst drops its extra notes
        slot->used = true;
        slot->released = slot->sustained = false;
        slot->at = blockStart + sampleInBlock + static_cast<int64_t>(std::llround(strum * (i + 1)));
        slot->channel = channel;
        slot->root = note;
        slot->note = note + macros.nitro.offsets[static_cast<size_t>(k)];
        slot->velocity = velocity;
        slot->gain = clamp(0.0f, 1.0f, macros.nitro.levels[static_cast<size_t>(k)]) * groupGain;
        slot->pan = n == 1 ? 0.0f : width * (-1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(n - 1));
        slot->cents = clamp(0.0f, 50.0f, macros.nitro.detuneCents) * (i % 2 == 0 ? 1.0f : -1.0f);
    }
}

void PhoqerEngine::renderTo(AudioBuffer& output, int& cursor, int target)
{
    for (;;)
    {
        PendingNote* due = nullptr;
        for (auto& p : pending)
            if (p.used && p.at - blockStart < target && (due == nullptr || p.at < due->at)) due = &p;
        if (due == nullptr) break;
        const int at = static_cast<int>(std::max<int64_t>(cursor, due->at - blockStart));
        renderSegment(output, cursor, at - cursor);
        cursor = at;
        const auto p = *due;
        due->used = false;
        if (auto* voice = nitroVoiceFor())
        {
            sustained[static_cast<size_t>(voice - voices.data())] = p.sustained;
            if (voice->isActive()) voice->stopNote(false);
            voice->startNote(p.channel, p.note, p.velocity, pitchWheels[static_cast<size_t>(p.channel)]);
            voice->setLayer(p.gain, p.pan, p.cents, p.channel, p.root);
            if (p.released) voice->stopNote(true);
        }
    }
    renderSegment(output, cursor, target - cursor);
    cursor = target;
}

// ---------------------------------------------------------------------------------------------- ARP
double PhoqerEngine::arpStepBeats() const noexcept
{
    static constexpr double beats[] { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0 };     // 1/4, 1/8, 1/16, 1/32, 1/8 triplets
    return beats[clamp(0, 4, macros.arp.rate)];
}

double PhoqerEngine::arpStepSamples() const noexcept
{
    return arpStepBeats() * 60.0 / static_cast<double>(macros.tempoBpm) * sampleRate;
}

double PhoqerEngine::arpBeatOf(int64_t step) const noexcept
{
    const double length = arpStepBeats();
    return static_cast<double>(step) * length + ((step & 1) != 0 ? clamp(0.0f, 1.0f, macros.arp.swing) * length / 3.0 : 0.0);
}

int64_t PhoqerEngine::arpSyncedNext() noexcept
{
    const double length = arpStepBeats(), now = macros.songBeats;
    auto step = static_cast<int64_t>(std::floor(std::max(now, arpLastBeat) / length)) - 1;
    while (arpBeatOf(step) <= arpLastBeat + 1.0e-9 || arpBeatOf(step) < now - 1.0e-9) ++step;
    arpNextBeat = arpBeatOf(step); arpNextStep = step;
    return blockStart + static_cast<int64_t>(std::llround((arpNextBeat - now) * 60.0 * sampleRate / static_cast<double>(macros.tempoBpm)));
}

// One cycle of the pattern: the keys (low to high, or in the order played), repeated an octave up per OCTAVES.
int PhoqerEngine::arpSequence(std::array<int, 48>& out) const noexcept
{
    std::array<ArpKey, 16> keys {};
    const int n = arpKeyCount;
    std::copy(arpKeys.begin(), arpKeys.begin() + n, keys.begin());
    const bool played = macros.arp.pattern == 4;
    std::sort(keys.begin(), keys.begin() + n, [played](const ArpKey& a, const ArpKey& b) { return played ? a.order < b.order : a.note < b.note; });
    int count = 0;
    for (int o = 0; o < clamp(1, 3, macros.arp.octaves); ++o)
        for (int k = 0; k < n; ++k)
            if (const int note = keys[static_cast<size_t>(k)].note + 12 * o; note <= 127 && count < 48) out[static_cast<size_t>(count++)] = note;
    if (macros.arp.pattern == 1) std::reverse(out.begin(), out.begin() + count);                // DOWN
    if (macros.arp.pattern == 2 && count > 2)                                                    // UP + DOWN: back down without repeating the ends
        for (int i = count - 2; i >= 1 && count < 48; --i) out[static_cast<size_t>(count++)] = out[static_cast<size_t>(i)];
    return count;
}

void PhoqerEngine::arpKey(const MidiEvent& event, int sampleInBlock) noexcept
{
    const bool down = event.type == MidiEventType::noteOn && event.value > 0.0f;
    const int channel = clamp(0, 15, event.channel);
    int found = -1;
    for (int k = 0; k < arpKeyCount; ++k) if (arpKeys[static_cast<size_t>(k)].note == event.note && arpKeys[static_cast<size_t>(k)].channel == channel) found = k;
    if (down)
    {
        bool anyDown = false;
        for (int k = 0; k < arpKeyCount; ++k) anyDown = anyDown || arpKeys[static_cast<size_t>(k)].down;
        if (macros.arp.hold && ! anyDown && found < 0) arpKeyCount = 0;     // HOLD: a new chord after letting go replaces the held one
        const bool wasIdle = arpKeyCount == 0;
        if (found < 0 && arpKeyCount < static_cast<int>(arpKeys.size())) found = arpKeyCount++;
        if (found < 0) return;
        arpKeys[static_cast<size_t>(found)] = { event.note, channel, arpOrder++, clamp(0.0f, 1.0f, event.value), true };
        if (wasIdle)     // the first key: the pattern starts on this sample, or on the song's grid while it plays
        {
            arpStep = 0;
            if (macros.songPlaying)
            {
                // A chord up to a quarter of a step late still starts on its step (played now); later than that,
                // it waits for the next step, so every note lands on the grid.
                const double length = arpStepBeats(), at = macros.songBeats + sampleInBlock * static_cast<double>(macros.tempoBpm) / (60.0 * sampleRate);
                const auto step = static_cast<int64_t>(std::floor(at / length));
                if (at >= arpBeatOf(step) && at - arpBeatOf(step) <= length * 0.25)
                {
                    arpNextBeat = arpBeatOf(step); arpNextStep = step; arpLastBeat = arpNextBeat - 1.0e-6;
                    arpNextAt = blockStart + sampleInBlock;
                }
                else
                {
                    arpLastBeat = at;
                    arpNextAt = arpSyncedNext();
                }
            }
            else
            {
                arpGrid = static_cast<double>(blockStart + sampleInBlock);
                arpNextAt = blockStart + sampleInBlock;
            }
        }
        return;
    }
    if (found < 0) return;
    arpKeys[static_cast<size_t>(found)].down = false;
    if (! macros.arp.hold)
    {
        for (int k = found; k + 1 < arpKeyCount; ++k) arpKeys[static_cast<size_t>(k)] = arpKeys[static_cast<size_t>(k + 1)];
        --arpKeyCount;
        if (arpKeyCount == 0) { arpNextAt = -1; arpLane = -1; }     // all keys up: no new steps; the sounding one ends at its own time
    }
}

void PhoqerEngine::arpClear(int sampleInBlock) noexcept
{
    if (arpSounding >= 0) handleEvent({ MidiEventType::noteOff, sampleInBlock, arpSoundingChannel, arpSounding, 0.0f }, sampleInBlock);
    arpSounding = -1; arpKeyCount = 0; arpNextAt = arpOffAt = -1; arpLane = -1;
}

void PhoqerEngine::arpRun(AudioBuffer& output, int& cursor, int target)
{
    for (;;)
    {
        const int64_t never = std::numeric_limits<int64_t>::max();
        const int64_t off = arpOffAt >= 0 ? arpOffAt : never, next = arpNextAt >= 0 ? arpNextAt : never;
        const int64_t due = std::min(off, next);
        if (due == never || due - blockStart >= target) break;
        const int at = static_cast<int>(std::max<int64_t>(cursor, due - blockStart));
        renderTo(output, cursor, at);
        if (off <= next)     // the sounding step note ends
        {
            if (arpSounding >= 0) handleEvent({ MidiEventType::noteOff, at, arpSoundingChannel, arpSounding, 0.0f }, at);
            arpSounding = -1; arpOffAt = -1;
            continue;
        }
        if (! macros.arp.hold)     // HOLD switched off with keys only held by it: they go now
        {
            int kept = 0;
            for (int k = 0; k < arpKeyCount; ++k) if (arpKeys[static_cast<size_t>(k)].down) arpKeys[static_cast<size_t>(kept++)] = arpKeys[static_cast<size_t>(k)];
            arpKeyCount = kept;
        }
        std::array<int, 48> seq {};
        const int count = arpSequence(seq);
        if (count == 0) { arpNextAt = -1; arpLane = -1; continue; }
        if (arpSounding >= 0) handleEvent({ MidiEventType::noteOff, at, arpSoundingChannel, arpSounding, 0.0f }, at);
        int pick = arpStep % count;
        if (macros.arp.pattern == 3) { arpRandom = arpRandom * 1664525u + 1013904223u; pick = static_cast<int>((arpRandom >> 8) % static_cast<uint32_t>(count)); }
        const int note = seq[static_cast<size_t>(pick)];
        const ArpKey* source = &arpKeys[0];     // the velocity and channel of the key this note comes from
        for (int k = 0; k < arpKeyCount; ++k) if ((note - arpKeys[static_cast<size_t>(k)].note) % 12 == 0 && note >= arpKeys[static_cast<size_t>(k)].note) source = &arpKeys[static_cast<size_t>(k)];
        const double step = arpStepSamples();
        // The step lanes: 16 steps that loop on the song's bar while it plays (from the first key otherwise),
        // each with its loudness, its length (times GATE) or a rest.
        arpLane = static_cast<int>((macros.songPlaying ? arpNextStep : static_cast<int64_t>(arpStep)) & (arpLaneSteps - 1));
        const auto& lane = macros.arp.lanes[static_cast<size_t>(arpLane)];
        arpLastStepAt = blockStart + at;
        if (lane.rest) { arpSounding = -1; arpOffAt = -1; }
        else
        {
            handleEvent({ MidiEventType::noteOn, at, source->channel, note, source->velocity * clamp(0.02f, 1.0f, lane.velocity) }, at);
            arpSounding = note; arpSoundingChannel = source->channel;
            arpOffAt = due + std::max<int64_t>(1, static_cast<int64_t>(std::llround(clamp(0.05f, 1.0f, macros.arp.gate) * clamp(0.05f, 1.0f, lane.gate) * step)));
        }
        ++arpStep;
        if (macros.songPlaying)     // the next step on the song's grid (SWING there follows the song's beats)
        {
            arpLastBeat = arpNextBeat;
            arpNextAt = arpSyncedNext();
        }
        else
        {
            arpGrid += step;     // every second step is pushed late by SWING, up to a third of a step
            const double late = (arpStep % 2 == 1) ? clamp(0.0f, 1.0f, macros.arp.swing) * step / 3.0 : 0.0;
            arpNextAt = static_cast<int64_t>(std::llround(arpGrid + late));
        }
        if (arpOffAt > arpNextAt && arpNextAt >= 0) arpOffAt = arpNextAt;     // never longer than the step
    }
    renderTo(output, cursor, target);
}

// YAP: VOWEL steps through a talking pattern on 8th notes at the tempo, round wherever the VOWEL knob is (low:
// darker "oo" talk, high: brighter "ah" talk), eased (about 11 ms) so each step slides into the next. It starts
// from the top whenever the seal has been quiet.
void PhoqerEngine::renderSegment(AudioBuffer& output, int startSample, int numSamples)
{
    if (! yapActive()) { renderVoices(output, startSample, numSamples); return; }
    static constexpr float pattern[] { 0.05f, 0.9f, 0.35f, 0.95f, 0.1f, 0.6f, 0.95f, 0.3f };
    while (numSamples > 0)
    {
        const int n = std::min(numSamples, 64);
        if (getActiveVoiceCount() == 0) { yapBeats = 0.0; yapVowel = macros.vowel; }
        // while the song plays, the talking pattern sits on its 8th notes; otherwise it counts from the note
        const double beats = macros.songPlaying ? macros.songBeats + startSample * static_cast<double>(macros.tempoBpm) / (60.0 * sampleRate) : yapBeats;
        const auto step = static_cast<size_t>(((static_cast<int64_t>(std::floor(beats * 2.0)) % 8) + 8) % 8);
        const float knob = macros.vowel, swing = clamp(0.25f, 0.45f, std::min(knob, 1.0f - knob));
        const float target = clamp(0.0f, 1.0f, knob + (pattern[step] - 0.5f) * 2.0f * swing);
        yapVowel += (target - yapVowel) * (1.0f - std::exp(-static_cast<float>(n / sampleRate) / 0.0114f));
        yapBeats += n * static_cast<double>(macros.tempoBpm) / (60.0 * sampleRate);
        auto moved = macros;
        moved.vowel = yapVowel;
        for (auto& voice : voices) voice.setMacros(moved);
        renderVoices(output, startSample, n);
        startSample += n;
        numSamples -= n;
    }
}

void PhoqerEngine::process(AudioBuffer& output, const MidiEvent* events, int eventCount,
                           const MacroState& inputMacros, float outputDecibels)
{
    output.clear();
    const auto requestedCharacter = isKnownSealCharacter(inputMacros.character)
        ? inputMacros.character
        : defaultSealCharacter;
    // Each voice captures its character preset at note-on, so switching CHARACTER
    // affects only new notes. Held notes keep their identity rather than being
    // cut off, which matters now that a note sustains for as long as it is held.
    activeCharacter = requestedCharacter;

    // A full copy, then each field that needs a limit set by name: a field added to MacroState later
    // is carried through as it is, never silently reset to its default.
    macros = inputMacros;
    macros.boom = clamp(0.0f, 1.0f, inputMacros.boom);
    macros.air = clamp(0.0f, 1.0f, inputMacros.air);
    macros.bark = clamp(0.0f, 1.0f, inputMacros.bark);
    macros.vowel = clamp(0.0f, 1.0f, inputMacros.vowel);
    macros.space = clamp(0.0f, 1.0f, inputMacros.space);
    // the mod wheel adds expression on top of the TIDE knob, all the way to full
    macros.tide = clamp(0.0f, 1.0f, inputMacros.tide + modWheel * (1.0f - inputMacros.tide));
    macros.detune = clamp(0.0f, 1.0f, inputMacros.detune);
    macros.character = activeCharacter;
    macros.tempoBpm = clamp(20.0f, 400.0f, inputMacros.tempoBpm);
    macros.glideMs = clamp(0.0f, 2000.0f, inputMacros.glideMs);
    macros.wailBend = clamp(0.0f, 24.0f, inputMacros.wailBend);
    for (auto& voice : voices)
        voice.setMacros(macros);

    blockStart = clock;
    int cursor = 0;
    if (arpWasActive && ! arpActive()) arpClear(0);     // ARP switched off: its note ends, the collected keys are forgotten
    arpWasActive = arpActive();
    if (arpActive() && arpNextAt >= 0)
    {
        if (macros.songPlaying)     // every block: a tempo, RATE or SWING change, or a jump in the song, never pulls it off the grid
        {
            if (arpLastBeat > macros.songBeats + arpStepBeats() * 0.5) arpLastBeat = -1.0e9;     // the song jumped back (a loop)
            arpNextAt = arpSyncedNext();
        }
        else if (arpWasSynced)      // the song stopped: carry on freely from the step that was due
            arpGrid = static_cast<double>(arpNextAt);
    }
    arpWasSynced = macros.songPlaying;
    // Asleep: no MIDI, nothing sounding or due, and a second of output below -100 dB (the SPACE tail included):
    // the voices and effects are skipped, so an idle PHOQER costs next to nothing. Any MIDI wakes it.
    if (eventCount == 0 && silentSamples >= static_cast<int64_t>(sampleRate) && ! busy())
    {
        clock += output.getNumSamples();     // output was cleared above
        telemetry.publishMeter(0.0f, 0.0f);
        return;
    }
    for (int index = 0; index < eventCount; ++index)
    {
        const auto eventSample = clamp(cursor, output.getNumSamples(), events[index].sampleOffset);
        const auto& event = events[index];
        if (arpActive()) arpRun(output, cursor, eventSample); else renderTo(output, cursor, eventSample);
        if (event.type == MidiEventType::allNotesOff) arpClear(eventSample);
        if (arpActive() && (event.type == MidiEventType::noteOn || event.type == MidiEventType::noteOff)) arpKey(event, eventSample);
        else handleEvent(event, eventSample);
    }
    if (arpActive()) arpRun(output, cursor, output.getNumSamples()); else renderTo(output, cursor, output.getNumSamples());
    clock += output.getNumSamples();

    chorusStage.process(output, macros.detune);
    spaceStage.process(output, macros.space);
    fishSong.process(output);
    outputStage.setOutputDb(outputDecibels);
    outputStage.process(output);
    publishTelemetry(output);
    silentSamples = output.getMagnitude(0, output.getNumSamples()) < 1.0e-5f ? silentSamples + output.getNumSamples() : 0;
}

bool PhoqerEngine::busy() const noexcept
{
    if (getActiveVoiceCount() > 0 || fishSong.isPlaying() || arpSounding >= 0 || (arpActive() && arpNextAt >= 0)) return true;
    for (const auto& p : pending) if (p.used) return true;
    return false;
}

void PhoqerEngine::publishTelemetry(const AudioBuffer& output) noexcept
{
    const SealVoice* dominant = nullptr;
    float bestScore = 0.0f;
    for (const auto& voice : voices)
    {
        const auto& state = voice.getTelemetry();
        if (! state.active)
            continue;
        const auto recency = 1.0f + 0.05f / (1.0f + static_cast<float>(ageCounter - state.age));
        const auto score = state.envelope * (0.4f + 0.6f * state.velocity) * recency;
        if (score > bestScore)
        {
            dominant = &voice;
            bestScore = score;
        }
    }

    FaceTelemetry face;
    if (dominant != nullptr)
    {
        const auto& state = dominant->getTelemetry();
        const auto& vocal = state.vocal;
        face.mouthOpen = vocal.mouthOpen;
        face.mouthRound = vocal.mouthRound;
        face.jawWidth = clamp(0.0f, 1.0f,
            0.10f + 0.56f * vocal.mouthOpen + 0.20f * (1.0f - vocal.mouthRound)
            + 0.22f * vocal.barkTransient);
        face.throatTension = vocal.throatPressure;
        face.eyeSquint = clamp(0.0f, 1.0f, vocal.barkTransient * 0.9f);
        face.eyeOpen = clamp(0.0f, 1.0f,
            0.18f * vocal.callIntensity + 0.58f * vocal.pitchLift + 0.24f * state.registerPosition);
        face.headLift = clamp(0.0f, 1.0f,
            0.34f * state.registerPosition + 0.50f * vocal.pitchLift + 0.16f * vocal.callPhase);
        face.intensity = clamp(0.0f, 1.0f, vocal.callIntensity + 0.1f * outputStage.getRms());
    }
    telemetry.publishFace(face);
    telemetry.publishMeter(outputStage.getPeak(), outputStage.getRms());

    if (output.getNumChannels() == 0)
        return;
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
    {
        const auto mono = output.getNumChannels() > 1
            ? 0.5f * (output.getSample(0, sample) + output.getSample(1, sample))
            : output.getSample(0, sample);
        telemetry.pushWaveform(mono);
    }
}
}
