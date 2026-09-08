# PHOQER development notes

PHOQER 0.1.0-dev is a C++17/JUCE 8 MIDI instrument. The project separates a
framework-neutral synthesis engine from its JUCE host and editor adapters.

## Architecture

`PhoqerEngine` owns eight fixed voices, sample-accurate MIDI dispatch, character
selection, room processing, transparent output gain, and lock-free telemetry. It has no
JUCE dependency. `PhoqerAudioProcessor` translates JUCE audio, MIDI, parameters, and
state into that core without allocation or string lookup in the sample-processing path.

The engine defines Low / Burp, Main / Bark-Groan, and Pad / Moan-Shout character slots.
Only Main is implemented; Low and Pad remain intentionally silent until their reference
audio and synthesis designs are approved.

The editor uses APVTS attachments for controls. A 30 Hz timer reads the fixed waveform
ring, meter atomics, and normalized face telemetry. The audio callback never calls UI,
filesystem, or message-thread code.

Stable parameter IDs are `boom`, `air`, `bark`, `vowel`, `space`, `tide`, `detune`,
`output`, `character`, and `behavior`. The `space` ID is displayed as REVERB. Visual
identity is derived from `character` and is not stored as an independent parameter.

## Validation

Build and run `PHOQERCoreSanity` for offline engine checks. Validate the complete VST3
bundle with Steinberg's validator before distributing it. Test VST3 and Standalone at
44.1, 48, 88.2, and 96 kHz with block sizes including 32, 64, 128, 512, and 1024.

## Licensing

Original PHOQER material is `AGPL-3.0-only`. JUCE 8.0.8 is used under its AGPLv3 option.
Third-party components retain their own notices; see `THIRD_PARTY_NOTICES.md` and
`vendor/JUCE/LICENSE.md`.
