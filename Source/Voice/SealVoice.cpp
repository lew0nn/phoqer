#include "SealVoice.h"

#include <algorithm>
#include <cmath>

namespace phoqer
{
using gestures::modeMotion;
using gestures::belchPitch;
using gestures::belchGain;
using gestures::belchSeconds;
using levels::modeTrimDb;

SealVoice::SealVoice(uint32_t seed, uint64_t& globalAgeCounter) noexcept
    : random(seed), ageCounter(globalAgeCounter)
{
    // A fixed per-voice pan and detune direction. Eight voices then decorrelate
    // on their own, which is what makes the dry path stereo without a widener.
    const auto panPosition = random.range(-0.6f, 0.6f);
    panLeft = std::sqrt(0.5f * (1.0f - panPosition));
    panRight = std::sqrt(0.5f * (1.0f + panPosition));
    homePanLeft = panLeft;
    homePanRight = panRight;
    detuneOffsetSemitones = random.bipolar();
}

void SealVoice::setLayer(float gain, float panPosition, float cents, int channel, int note) noexcept
{
    layerGain = gain;
    panPosition = clamp(-1.0f, 1.0f, panPosition);
    panLeft = std::sqrt(0.5f * (1.0f - panPosition));
    panRight = std::sqrt(0.5f * (1.0f + panPosition));
    layerSemitones = cents * 0.01f;
    parentChannel = channel;
    parentNote = note;
}

void SealVoice::prepare(double newSampleRate, int)
{
    sampleRate = newSampleRate;
    amplitudeEnvelope.setSampleRate(sampleRate);
    fofBank.prepare(sampleRate);
    // A gentle 7 kHz roll-off on each voice: the reference calls carry almost
    // nothing up there, and what the grains put there reads as digital fizz.
    polishCoefficient = 1.0f - std::exp(-twoPi<float> * 7000.0f / static_cast<float>(sampleRate));
    muffleCoefficient = 1.0f - std::exp(-twoPi<float> * 900.0f / static_cast<float>(sampleRate));
    for (auto* smoother : { &smoothBoom, &smoothAir, &smoothBark, &smoothVowel,
                            &smoothTide, &smoothDetune })
        smoother->reset(sampleRate, 0.035);
    smoothBoom.setCurrentAndTargetValue(macros.boom);
    smoothAir.setCurrentAndTargetValue(macros.air);
    smoothBark.setCurrentAndTargetValue(macros.bark);
    smoothVowel.setCurrentAndTargetValue(macros.vowel);
    smoothTide.setCurrentAndTargetValue(macros.tide);
    smoothDetune.setCurrentAndTargetValue(macros.detune);
    smoothBend.reset(sampleRate, 0.005);
    smoothBend.setCurrentAndTargetValue(0.0f);
    smoothNote.reset(sampleRate, 0.0);
    hardReset();
}

void SealVoice::setMacros(const MacroState& newMacros) noexcept
{
    macros = newMacros;
    smoothBoom.setTargetValue(macros.boom);
    smoothAir.setTargetValue(macros.air);
    smoothBark.setTargetValue(macros.bark);
    smoothVowel.setTargetValue(macros.vowel);
    smoothTide.setTargetValue(macros.tide);
    smoothDetune.setTargetValue(macros.detune);
}

void SealVoice::resetDsp() noexcept
{
    fofBank.reset();
    previousOutput = 0.0f;
    polish = 0.0f;
    muffled = 0.0f;
    telemetry = {};
}

void SealVoice::deactivate() noexcept
{
    resetDsp();
    active = false;
    releasing = false;
    fading = false;
}

void SealVoice::hardReset() noexcept
{
    amplitudeEnvelope.reset();
    stolenTail = 0.0f;
    stolenTailSamples = 0;
    noteAgeSeconds = 0.0f;
    noteBeats = 0.0;
    deactivate();
}

void SealVoice::startNote(int midiChannel, int midiNoteNumber, float velocity,
                          int currentPitchWheelPosition)
{
    // A silent voice's knob smoothers never moved (they only step while it plays), so a knob turned
    // between notes would otherwise start the next note on its old value and slide.
    if (! active)
    {
        smoothBoom.setCurrentAndTargetValue(macros.boom);
        smoothAir.setCurrentAndTargetValue(macros.air);
        smoothBark.setCurrentAndTargetValue(macros.bark);
        smoothVowel.setCurrentAndTargetValue(macros.vowel);
        smoothTide.setCurrentAndTargetValue(macros.tide);
        smoothDetune.setCurrentAndTargetValue(macros.detune);
    }
    resetDsp();
    belch = false;
    fading = false;
    mouthGate = 0.0f;
    currentMidiChannel = midiChannel;
    currentMidiNote = midiNoteNumber;
    currentVelocity = clamp(0.0f, 1.0f, velocity);
    panLeft = homePanLeft;
    panRight = homePanRight;
    layerGain = 1.0f;
    layerSemitones = 0.0f;
    parentChannel = parentNote = -1;
    personality = VoicePersonality::create(random);
    if (macros.loadedVoice != nullptr)
    {
        capturedPreset = *macros.loadedVoice;
        preset = &capturedPreset;
    }
    else preset = &characterPreset(macros.character);
    levelSlot = macros.loadedVoice != nullptr ? 3 : std::min(2, static_cast<int>(macros.character));
    wobble.prepare(sampleRate, preset->wobbleRateHz, &random);
    flutterDrift.prepare(sampleRate, 7.0f, &random);
    mutter.prepare(sampleRate, 7.0f, &random);
    muffled = 0.0f;
    muffleAmount = macros.behaviorMode == BehaviourMode::murmur ? 1.0f : 0.0f;
    flutterPhase = random.nextFloat();
    wailHoldAmount = macros.wailHold ? 1.0f : 0.0f;     // a note started with the lock on is locked from the start
    thumpPhase = 0.0;
    // BARK scales the punch and the kick, 0.15x to 2.6x (1x at the default 0.35).
    barkScale = 0.15f + 2.43f * macros.bark;
    vibratoPhase = 0.0;
    behaviour.start(currentVelocity, macros, personality, preset->onsetPunch * barkScale);
    held = {};
    toShape = 0.0f;     // silent at the very start, as the attack gate is
    toBark = 0.0f;
    controlCountdown = 0;
    controlFresh = true;

    baseFrequency = midiNoteToHz(static_cast<float>(currentMidiNote));
    renderedPitchNote = static_cast<float>(currentMidiNote);
    smoothNote.setCurrentAndTargetValue(static_cast<float>(currentMidiNote));
    fofBank.setNoteFrequency(baseFrequency);
    noteAgeSeconds = 0.0f;
    noteBeats = 0.0;
    pitchWheelMoved(currentPitchWheelPosition);
    smoothBend.setCurrentAndTargetValue(pitchWheelSemitones);

    // ADSR comes from the character preset. BARK stretches or tightens the
    // attack: twice as slow at 0, a quarter at 1, as measured at the default.
    AdsrEnvelope::Parameters envelope;
    envelope.attack = preset->attackSeconds * std::exp2((0.35f - macros.bark) * 3.0f);
    envelope.decay = preset->decaySeconds;
    envelope.sustain = preset->sustainLevel;
    envelope.release = preset->releaseSeconds;
    amplitudeEnvelope.setParameters(envelope);
    amplitudeEnvelope.noteOn();

    telemetry.velocity = currentVelocity;
    telemetry.registerPosition = clamp(0.0f, 1.0f, (currentMidiNote - 36.0f) / 60.0f);
    telemetry.age = ++ageCounter;
    telemetry.active = true;
    active = true;
    releasing = false;
}

// MONO legato: the note carries on and moves to the new key over GLIDE (5 ms at least, so a jump doesn't click).
void SealVoice::legatoTo(int midiChannel, int midiNoteNumber) noexcept
{
    const float from = smoothNote.getCurrentValue();
    currentMidiChannel = midiChannel;
    currentMidiNote = midiNoteNumber;
    smoothNote.reset(sampleRate, std::max(macros.glideMs, 5.0f) * 0.001);
    smoothNote.setCurrentAndTargetValue(from);
    smoothNote.setTargetValue(static_cast<float>(midiNoteNumber));
    telemetry.registerPosition = clamp(0.0f, 1.0f, (currentMidiNote - 36.0f) / 60.0f);
}

void SealVoice::glideFrom(float fromNote) noexcept
{
    if (macros.glideMs <= 0.0f) return;
    smoothNote.reset(sampleRate, macros.glideMs * 0.001);
    smoothNote.setCurrentAndTargetValue(fromNote);
    smoothNote.setTargetValue(static_cast<float>(currentMidiNote));
}

// Overfed: a "glk" gulp, then a long rattling belch an octave down that sags as it goes.
void SealVoice::startBelch(int midiNoteNumber, float velocity)
{
    startNote(0, midiNoteNumber - 12, velocity, 8192);
    belch = true;
}

void SealVoice::fadeOut() noexcept
{
    if (! active)
        return;
    amplitudeEnvelope.releaseIn(0.003f);
    releasing = true;
    fading = true;
}

void SealVoice::stopNote(bool allowTailOff)
{
    if (! active)
        return;

    if (allowTailOff)
    {
        amplitudeEnvelope.noteOff();
        behaviour.noteOff();
        releasing = true;
        return;
    }

    stolenTail = previousOutput;
    stolenTailSamples = 32;
    amplitudeEnvelope.reset();
    deactivate();
}

void SealVoice::pitchWheelMoved(int value) noexcept
{
    pitchWheelPosition = clamp(0, 16383, value);
    const auto centered = pitchWheelPosition - 8192;
    pitchWheelSemitones = static_cast<float>(centered) / (centered >= 0 ? 8191.0f : 8192.0f)
                        * 2.0f;     // two semitones each way
    smoothBend.setTargetValue(pitchWheelSemitones);
}

void SealVoice::renderNextBlock(AudioBuffer& output, int startSample, int numSamples)
{
    if (! active)
        return;

    const auto dt = static_cast<float>(1.0 / sampleRate);
    const auto beatStep = static_cast<double>(macros.tempoBpm) / (60.0 * sampleRate);
    const auto channelCount = output.getNumChannels();
    auto& c = held;

    for (int offset = 0; offset < numSamples; ++offset)
    {
        // Every sample: the envelope, the note's age and the random streams, which draw on the same
        // samples whatever the control rate (the grains and the breath share their generator).
        c.envelope = amplitudeEnvelope.getNextSample();
        ageNote(dt, beatStep);
        const auto wobbleNow = wobble.next();
        const auto flutterNow = preset->flutterSemitones > 0.0f ? flutterDrift.next() : 0.0f;
        if (controlCountdown == 0)
        {
            updateControl(dt * controlStep, wobbleNow, flutterNow);
            controlCountdown = controlStep;
        }
        const auto blend = 1.0f - static_cast<float>(--controlCountdown) / controlStep;
        c.frequency = fromFrequency + static_cast<double>(blend) * (toFrequency - fromFrequency);
        c.vocal.amplitudeShape = lerp(blend, fromShape, toShape);
        c.vocal.barkTransient = lerp(blend, fromBark, toBark);
        c.modeGain = lerp(blend, fromGain, toGain);
        if (c.motion.muffle > 0.0f)    // the mutter: random syllables, about seven a second, as deep as the mouth is closed
        {                              // (all the way on MURMUR, about half on HONK's half-closed mouth)
            const auto syllable = 0.15f + 0.85f * clamp(0.0f, 1.0f, 0.55f + 0.9f * mutter.next());
            c.modeGain *= c.motion.muffle >= 1.0f ? syllable : 1.0f - c.motion.muffle * (1.0f - syllable);
        }

        const auto value = nextSample(c, dt) * layerGain;
        const auto sampleIndex = startSample + offset;
        if (channelCount > 1)
        {
            output.addSample(0, sampleIndex, value * panLeft);
            output.addSample(1, sampleIndex, value * panRight);
            for (int channel = 2; channel < channelCount; ++channel)
                output.addSample(channel, sampleIndex, value);
        }
        else if (channelCount == 1)
        {
            output.addSample(0, sampleIndex, value);
        }

        // The face follows the mode's rhythm: each hit opens the mouth and it closes over ~180 ms,
        // long enough for a 30 Hz screen to show a 70 ms honk.
        mouthGate = std::max(clamp(0.0f, 1.0f, c.motion.gain), mouthGate - dt / 0.18f);
        if (! amplitudeEnvelope.isActive())     // the note has ended: stop on this sample, not at the end of the block,
            break;                              // so the output and the random draws don't depend on the host's block size
    }
    // The face reads the voice once a block.
    telemetry.vocal = c.vocal;
    telemetry.vocal.mouthOpen *= mouthGate;
    telemetry.envelope = c.envelope;
    telemetry.active = true;

    if (! amplitudeEnvelope.isActive())
        deactivate();
}

// The note's motion at this control point: the smoothed knobs, the call's shape, its pitch, mouth and
// level, into held. Everything here is control; nothing makes sound, and nothing draws a random number
// (the random values it needs are drawn every sample, in renderNextBlock). controlDt: the control
// step, in seconds.
void SealVoice::updateControl(float controlDt, float wobbleNow, float flutterNow) noexcept
{
    auto& c = held;
    c.macros = smoothedMacros();
    // Key force growls: a hard key pushes BARK up, a soft one pulls it down, all through the note (0.6 is the
    // middle; owner-approved by ear on 12 Nov 2026). The onset bark already scales with velocity in BehaviourEngine.
    c.macros.bark = clamp(0.0f, 1.0f, c.macros.bark + (currentVelocity - 0.6f) * 0.6f);
    c.vocal = behaviour.process(controlDt, c.macros, c.envelope, 0.0f);
    // TIDE is expression: 0 plays a dead-straight note, the default 0.25 the
    // measured call, 1 more than twice as much movement plus a vibrato.
    const auto tide = c.macros.tide;
    const auto expression = tide < 0.25f ? tide * 4.0f : 1.0f + (tide - 0.25f) * 1.6f;
    auto pitch = pitchMotion(c, controlDt, expression, wobbleNow, flutterNow);
    c.motion = moodMotion(c.macros, controlDt);
    mouthShape(c);
    c.vocal.barkTransient += c.motion.kick * barkScale;
    pitch.drift += c.motion.semitones;
    modeSemitones = c.motion.semitones;

    fromShape = toShape;           toShape = c.vocal.amplitudeShape;
    fromBark = toBark;             toBark = c.vocal.barkTransient;
    fromGain = toGain;             toGain = moodLevel(c);
    fromFrequency = toFrequency;   toFrequency = noteFrequency(pitch);
    fofBank.setFundamental(toFrequency);
    if (controlFresh)    // the first point of a note: pitch and level start where they are
    {
        fromGain = toGain;
        fromFrequency = toFrequency;
        controlFresh = false;
    }
}

// The knobs at this control point, each set by name; the smoothers step a whole control step.
MacroState SealVoice::smoothedMacros() noexcept
{
    auto m = macros;
    m.boom = smoothBoom.skip(controlStep);
    m.air = smoothAir.skip(controlStep);
    m.bark = smoothBark.skip(controlStep);
    m.vowel = smoothVowel.skip(controlStep);
    m.tide = smoothTide.skip(controlStep);
    m.detune = smoothDetune.skip(controlStep);
    return m;
}

void SealVoice::ageNote(float dt, double beatStep) noexcept
{
    noteAgeSeconds += dt;
    noteBeats += beatStep;
    if (belch && ! releasing && noteAgeSeconds > belchSeconds)
        stopNote(true);
}

// Where the pitch is, before the mood moves it: the onset gesture, DETUNE, and the drift on top.
SealVoice::PitchParts SealVoice::pitchMotion(SampleControl& c, float dt, float expression, float wobbleNow, float flutterNow) noexcept
{
    PitchParts p;
    // The onset pitch gesture comes from the character preset table, which
    // holds the contour measured from the reference recordings. It settles
    // to its last value and holds, so a sustained note stays in tune.
    const auto gestureLength = preset->gestureSeconds > 0.0f ? preset->gestureSeconds : 0.25f;
    p.contour = belch ? belchPitch(noteAgeSeconds)
                      : pitchContourAt(preset->pitchContour, noteAgeSeconds / gestureLength)
                      * preset->gestureDepth * 0.74f * expression;
    c.vocal.pitchLift = clamp(0.0f, 1.0f, (p.contour + 2.0f) * 0.25f);

    p.detune = detuneOffsetSemitones * c.macros.detune * 0.35f;
    // The measured calls never hold a pitch: a smoothed random drift on top
    // of the contour, deepest on SQUEAL.
    p.drift = wobbleNow * preset->wobbleSemitones * expression;
    // GROAN's flutter: about 12.5 Hz, its rate and depth drifting a little.
    if (preset->flutterSemitones > 0.0f)
    {
        const auto d = flutterNow;
        flutterPhase += preset->flutterRateHz * (1.0f + 0.30f * d) * dt;
        flutterPhase = wrapPhase(flutterPhase);
        p.drift += fastSin2Pi(flutterPhase)
                 * preset->flutterSemitones * (0.75f + 0.55f * d) * std::min(1.0f, expression);
    }
    const auto tide = c.macros.tide;
    if (tide > 0.4f)    // a singer's vibrato, 5.5 Hz, fading in after the attack
    {
        vibratoPhase += 5.5 * dt;
        vibratoPhase = wrapPhase(vibratoPhase);
        const auto fadeIn = clamp(0.0f, 1.0f, (noteAgeSeconds - 0.25f) / 0.35f);
        p.drift += fastSin2Pi(vibratoPhase)
                 * 0.45f * (tide - 0.4f) / 0.6f * fadeIn;
    }
    return p;
}

// What the mood does to the held note right now (see Gestures.h). The WAIL lock eases over 150 ms
// when it is pressed or released during a note.
gestures::ModeMotion SealVoice::moodMotion(const MacroState& m, float dt) noexcept
{
    const auto holdTarget = m.wailHold ? 1.0f : 0.0f;
    if (wailHoldAmount != holdTarget)
        wailHoldAmount = holdTarget > wailHoldAmount ? std::min(holdTarget, wailHoldAmount + dt / 0.15f)
                                                     : std::max(holdTarget, wailHoldAmount - dt / 0.15f);
    return modeMotion(belch ? BehaviourMode::call : m.behaviorMode,
                      noteBeats, m.tempoBpm, m.wailBend, wailHoldAmount);
}

// The mouth: the call's vowel moved by the mood and the voice's own colour, or the vowel map's.
void SealVoice::mouthShape(SampleControl& c) noexcept
{
    auto& vocal = c.vocal;
    vocal.vowelMorph = clamp(0.0f, 1.0f, vocal.vowelMorph + c.motion.vowel + preset->vowelOffset);
    if(preset->fullVowelMap && !belch)
        vocal.vowelMorph=clamp(0.0f,1.0f,c.macros.vowel);
    if (belch)     // a closed "oo" gulp; the belch comes up as a wide "aw"
        vocal.vowelMorph = noteAgeSeconds < 0.18f ? 0.05f : 0.45f;
}

// How loud the mood makes the note: its own shape and the measured trim against CALL (MURMUR's mutter
// is added every sample, in renderNextBlock).
float SealVoice::moodLevel(const SampleControl& c) noexcept
{
    auto gain = c.motion.gain * (belch ? belchGain(noteAgeSeconds) : 1.0f);
    if (!belch)
    {
        const auto mode = static_cast<int>(c.macros.behaviorMode);
        if (mode != trimMode || levelSlot != trimSlot)     // the mood's trim, worked out once per mood
        {
            trimMode = mode;
            trimSlot = levelSlot;
            trimGain = fastExp2(modeTrimDb[levelSlot][mode] / 6.0206f);
        }
        gain *= trimGain;
    }
    return gain;
}

// The note's frequency: the played note (gliding when GLIDE is on), the gesture, the pitch wheel,
// DETUNE and the drift.
double SealVoice::noteFrequency(const PitchParts& p) noexcept
{
    const auto pitchNote = smoothNote.skip(controlStep);
    if (pitchNote != renderedPitchNote)
    {
        renderedPitchNote = pitchNote;
        baseFrequency = midiNoteToHz(pitchNote);
    }
    return static_cast<double>(baseFrequency)
        * fastExp2((p.contour + smoothBend.skip(controlStep) + p.detune + p.drift + layerSemitones) / 12.0f);
}

// One sample of sound from that control: the grains, the note's level, the kick, the roll-offs.
float SealVoice::nextSample(const SampleControl& c, float dt) noexcept
{
    const auto& vocal = c.vocal;
    auto value = fofBank.process(c.frequency, *preset, c.macros, vocal, random);
    const auto attackPunch = 1.0f + 1.20f * vocal.barkTransient;
    // Headroom: one note at full velocity peaks near -10 dBFS, so eight
    // voices sum to just under full scale and the limiter only catches the
    // coherent worst case rather than working on every chord.
    value *= c.envelope * vocal.amplitudeShape * attackPunch * c.modeGain
           * (0.05f + 0.13f * currentVelocity);

    // The kick: a short sine that drops from twice the note to the note in
    // about 15 ms and dies in about 40, under the attack only. It starts at
    // zero phase, so it adds a hit without a click.
    if (preset->thump > 0.0f && noteAgeSeconds < 0.25f)
    {
        const auto sweep = 1.0f + fastExp(-noteAgeSeconds / 0.015f);
        thumpPhase += baseFrequency * sweep * dt;
        thumpPhase = wrapPhase(thumpPhase);
        value += fastSin2Pi(thumpPhase)
               * preset->thump * barkScale * fastExp(-noteAgeSeconds / 0.040f)
               * (0.05f + 0.13f * currentVelocity) * 1.6f;
    }

    polish += polishCoefficient * (value - polish);
    value = polish;
    // MURMUR's closed mouth: a second, much lower roll-off (about 900 Hz), eased in
    muffleAmount += 0.002f * (c.motion.muffle - muffleAmount);
    muffled += muffleCoefficient * (value - muffled);
    value = lerp(muffleAmount, value, muffled * 1.6f);

    if (stolenTailSamples > 0)
    {
        value += stolenTail * static_cast<float>(stolenTailSamples) / 32.0f;
        --stolenTailSamples;
    }

    if (! isFiniteSample(value))
    {
        value = 0.0f;
        resetDsp();
    }

    previousOutput = value;
    return value;
}
}
