# PHOQER development notes

PHOQER engineering version 0.1.0-dev is a C++17/JUCE 8.0.8 MIDI instrument. This repository currently contains the core engine and a disposable generic parameter editor only.

## Clone and build

```powershell
git clone --recurse-submodules <repository-url> PHOQER
cd PHOQER
cmake -S . -B Build -G "Visual Studio 17 2022" -A x64
cmake --build Build --config Release --target PHOQER_VST3 PHOQER_Standalone
```

For an existing clone, run `git submodule update --init --recursive`. JUCE is pinned as `vendor/JUCE` at tag 8.0.8.

## Architecture

`PhoqerEngine` owns eight preallocated JUCE synthesiser voices, global cheap-digital and room stages, output safety, and lock-free telemetry. Each `SealVoice` composes a multi-stage call progression, one of five internal pitch gestures, excitation, throat nonlinearity, and a four-way parallel formant bank. The tract interpolates five U/O/A/E/I-like seal-vowel anchors across frequency, Q, and gain while mouth openness remains behaviour-driven. Voice personality is generated deterministically at note start. The loudest/most recent active voice drives face state.

Public APVTS parameters have stable IDs: `boom`, `air`, `bark`, `vowel`, `space`, `tide`, and `output`. The six macros are normalized; output spans -24 to +6 dB.

Face telemetry is published as normalized atomics and may be polled by a future editor at 30-60 Hz. The output waveform uses a fixed single-writer/single-reader ring; meter peak and RMS are atomic.

Not implemented: final editor/artwork, behaviour profiles beyond CALL plus its BARK transient, presets, installers, licensing, and release automation.
