# PHOQER development notes

PHOQER 0.1.0-dev is a C++17/JUCE 8 MIDI instrument. The project separates a
framework-neutral synthesis engine from its JUCE host and editor adapters.

## Architecture

`PhoqerEngine` owns eight fixed voices, sample-accurate MIDI dispatch, character
selection, room processing, output gain with a lookahead limiter, and lock-free telemetry.
It has no JUCE dependency. `PhoqerAudioProcessor` translates JUCE audio, MIDI, parameters,
and state into that core without allocation or string lookup in the sample-processing path.

## Synthesis

Voices use formant-wave-function (FOF) synthesis. Each formant is built from grains
triggered once per fundamental period, where a grain is a damped sinusoid with a
raised-cosine attack skirt. A grain is run as a complex rotation with a decaying radius,
never as a call to `sin` and `exp`.

Bandwidth is the only pole parameter, so Q is never a coefficient and the resonator is
stable at any Q. That is what allows GROAN's 34 Hz singing formant, a Q of about 33, and
BURP's second formant at a Q of 53.

Grains overlap heavily: a 34 Hz bandwidth decays over about 66 ms, so at 640 Hz roughly 43
grains sound at once. Holding that many oscillators is unnecessary. Once a grain leaves its
attack skirt it is a pure exponential sharing the band's pole, and the recursion is linear,
so every tail-phase grain sums into one accumulator per band. Only grains still inside the
skirt need their own state, and eight slots cover the whole keyboard. The tail is a
recursion rather than a buffer, so it is never truncated.

A narrow formant only speaks when a harmonic lands inside it, so each band carries a
`harmonicLock` weight that pulls its centre onto the nearest harmonic of the played note.
The reference recordings behave the same way: GROAN's spectral peak sits on its second
harmonic.

All three characters are data, not branches. `Source/Core/CharacterPreset.h` holds one
table entry per character with five formant bands, pitch contour, ADSR, noise ratio,
jitter, shimmer and period-doubling probability, derived from measurements of the
reference recordings. Adding a character is a table entry.

Note lifetime belongs to the amplitude envelope alone. A held key sustains indefinitely;
`BehaviourEngine` now only shapes the onset gesture and drives face telemetry. Each voice
captures its character preset at note-on, so changing CHARACTER affects only new notes and
never cuts a held one.

The editor uses APVTS attachments for controls. A 60 Hz timer reads the waveform ring,
meter atomics, and normalized face telemetry. The audio callback never calls UI,
filesystem, or message-thread code.

Stable parameter IDs are `boom`, `air`, `bark`, `vowel`, `space`, `tide`, `detune`,
`output`, `character`, and `behavior`. The `space` ID is displayed as REVERB. Visual
identity is derived from `character` and is not stored as an independent parameter.

## Validation

Build and run `PHOQERCoreSanity` for offline engine checks. It asserts, among other
things, that all three characters are audible and sonically distinct, that a held note
still sounds after nine seconds and stops after release, and that the narrow formants
defining GROAN and BURP survive edits to the preset table.

Validate the complete VST3 bundle with Steinberg's validator before distributing it. Test
VST3 and Standalone at 44.1, 48, 88.2, and 96 kHz with block sizes including 32, 64, 128,
512, and 1024.

Note that the repository path must not contain parentheses. JUCE's Windows binary-data
generator is passed through MSBuild command files and fails with exit code 9009 otherwise,
which affects the plugin targets but not `PHOQERCoreSanity`.

## Licensing

Original PHOQER material is `AGPL-3.0-only`. JUCE 8.0.8 is used under its AGPLv3 option.
Third-party components retain their own notices; see `THIRD_PARTY_NOTICES.md` and
`vendor/JUCE/LICENSE.md`.
