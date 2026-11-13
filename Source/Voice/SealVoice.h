#pragma once

#include "../Core/AudioBuffer.h"
#include "../Core/DspPrimitives.h"
#include "../Core/PhoqerTypes.h"
#include "../Core/Random.h"
#include "../Core/CharacterPreset.h"
#include "../DSP/BehaviourEngine.h"
#include "../DSP/FofBank.h"
#include "../DSP/SmoothedRandom.h"
#include "VoicePersonality.h"
#include "Gestures.h"
#include "LevelTables.h"

namespace phoqer
{
class SealVoice final
{
public:
    SealVoice(uint32_t seed, uint64_t& globalAgeCounter) noexcept;

    void prepare(double sampleRate, int maximumBlockSize);
    void hardReset() noexcept;
    void setMacros(const MacroState& newMacros) noexcept;
    void startNote(int midiChannel, int midiNoteNumber, float velocity, int currentPitchWheelPosition);
    void glideFrom(float fromNote) noexcept;     // slide in from another note's pitch over the GLIDE time
    void startTreat(int midiNoteNumber, float velocity);     // the fed seal's gulp; ends itself
    void startBelch(int midiNoteNumber, float velocity);     // overfed: the gulp, then a long belch
    void stopNote(bool allowTailOff);
    void pitchWheelMoved(int newPitchWheelValue) noexcept;
    void renderNextBlock(AudioBuffer& output, int startSample, int numSamples);

    bool isActive() const noexcept { return active; }
    bool matchesNote(int midiChannel, int midiNoteNumber) const noexcept
    {
        return active && ! belch && ! fading && parentNote < 0 && currentMidiChannel == midiChannel && currentMidiNote == midiNoteNumber;
    }
    // NITRO: this voice is an extra note of the key (parentChannel, parentNote), at its own level, pan and
    // tuning, and ends with that key. Called right after startNote, which clears it.
    void setLayer(float gain, float panPosition, float cents, int parentChannel, int parentNote) noexcept;
    void setLevel(float gain) noexcept { layerGain = gain; }
    bool matchesLayerOf(int midiChannel, int midiNoteNumber) const noexcept
    {
        return active && ! fading && parentNote == midiNoteNumber && parentChannel == midiChannel;
    }
    bool isExtra() const noexcept { return parentNote >= 0; }     // a NITRO extra note
    bool matchesChannel(int midiChannel) const noexcept { return active && currentMidiChannel == midiChannel; }
    const VoiceTelemetry& getTelemetry() const noexcept { return telemetry; }
    float getEnvelopeLevel() const noexcept { return telemetry.envelope; }
    bool isReleasing() const noexcept { return releasing; }
    // Taken over by a new note: the sound carries on and fades out over 3 ms, then the voice frees itself.
    void fadeOut() noexcept;
    bool isFading() const noexcept { return fading; }
    uint64_t getAge() const noexcept { return telemetry.age; }
    float getPitchNote() const noexcept { return smoothNote.getCurrentValue(); }
    float getPitchBend() const noexcept { return smoothBend.getCurrentValue(); }
    float getNoteAge() const noexcept { return noteAgeSeconds; }
    float getModeSemitones() const noexcept { return modeSemitones; }     // the mode's pitch move right now (WAIL's sweep)

private:
    // One sample, in two halves: what the note does (pitch, vowel, the mode's level), then the sound.
    struct SampleControl
    {
        MacroState macros;
        VocalState vocal;
        gestures::ModeMotion motion;
        float envelope = 0.0f;
        float modeGain = 1.0f;
        double frequency = 0.0;
    };
    void updateControl(float controlDt, float wobbleNow, float flutterNow) noexcept;
    struct PitchParts { float contour = 0.0f, detune = 0.0f, drift = 0.0f; };     // semitones
    MacroState smoothedMacros() noexcept;
    void ageNote(float dt, double beatStep) noexcept;
    PitchParts pitchMotion(SampleControl&, float dt, float expression, float wobbleNow, float flutterNow) noexcept;
    gestures::ModeMotion moodMotion(const MacroState&, float dt) noexcept;
    void mouthShape(SampleControl&) noexcept;
    float moodLevel(const SampleControl&) noexcept;
    double noteFrequency(const PitchParts&) noexcept;
    float nextSample(const SampleControl& c, float dt) noexcept;

    void resetDsp() noexcept;
    void deactivate() noexcept;

    Random random;
    uint64_t& ageCounter;
    VoicePersonality personality;
    MacroState macros;
    BehaviourEngine behaviour;
    // The note's motion (knobs, call shape, pitch, mouth, mood level) moves slowly, so it is worked
    // out every controlStep samples into held; what reaches the sound directly (pitch, level,
    // amplitude and bark) is interpolated in between, from the previous control point to this one.
    static constexpr int controlStep = 16;
    int controlCountdown = 0;
    bool controlFresh = true;     // the note's first control point: nothing to interpolate from yet
    int levelSlot = 0;     // the row of modeTrimDb: BURP, SQUEAL, GROAN or the loaded voice
    SampleControl held;
    double fromFrequency = 0.0, toFrequency = 0.0;
    float fromGain = 1.0f, toGain = 1.0f;
    float fromShape = 0.0f, toShape = 0.0f;
    float fromBark = 0.0f, toBark = 0.0f;
    FofBank fofBank;
    AdsrEnvelope amplitudeEnvelope;
    SmoothedRandom wobble;   // the unsteady pitch of a real call, per character
    SmoothedRandom flutterDrift;   // keeps the flutter from sounding like an LFO
    double flutterPhase = 0.0;
    double thumpPhase = 0.0;      // the kick under the attack
    double vibratoPhase = 0.0;    // TIDE's vibrato, above 0.4
    float barkScale = 1.0f;       // BARK at note-on: how hard the punch and the kick hit
    float polish = 0.0f;          // one-pole low-pass state: takes the digital fizz off the top
    float polishCoefficient = 1.0f;
    SmoothedRandom mutter;        // MURMUR's syllables
    float muffled = 0.0f, muffleAmount = 0.0f, muffleCoefficient = 1.0f;
    LinearSmoother smoothBoom, smoothAir, smoothBark, smoothVowel, smoothTide, smoothDetune;
    LinearSmoother smoothNote, smoothBend;     // GLIDE, and pitch bend over 5 ms
    VoiceTelemetry telemetry;
    CharacterPreset capturedPreset;
    const CharacterPreset* preset = &characterPreset(defaultSealCharacter);
    double sampleRate = 44100.0;
    float baseFrequency = 261.63f;
    float renderedPitchNote = 60.0f;
    float modeSemitones = 0.0f;
    float wailHoldAmount = 0.0f;
    int trimMode = -1, trimSlot = -1;     // which mood trim trimGain holds
    float trimGain = 1.0f;          // the WAIL lock, eased between 0 and 1
    float noteAgeSeconds = 0.0f;
    double noteBeats = 0.0;
    float detuneOffsetSemitones = 0.0f;
    float panLeft = 0.70710678f;
    float panRight = 0.70710678f;
    float homePanLeft = 0.70710678f, homePanRight = 0.70710678f;     // the voice's own pan, back on every note
    float layerGain = 1.0f, layerSemitones = 0.0f;                     // NITRO's level and detune (1 and 0 otherwise)
    int parentChannel = -1, parentNote = -1;                           // NITRO: the key this extra note belongs to
    int currentMidiChannel = 0;
    int currentMidiNote = 60;
    float currentVelocity = 0.0f;
    float pitchWheelSemitones = 0.0f;
    int pitchWheelPosition = 8192;
    float previousOutput = 0.0f;
    float stolenTail = 0.0f;
    int stolenTailSamples = 0;
    bool active = false;
    bool releasing = false;
    bool belch = false;
    bool fading = false;          // fading out after being taken over (fadeOut)
    float mouthGate = 0.0f;       // the mode's hits, held a moment for the face
};
}
