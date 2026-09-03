# PHOQER development notes

PHOQER engineering version 0.1.0-dev is a C++17/iPlug2 MIDI instrument. The repository contains a framework-independent seal synthesis engine and an intentionally restrained iPlug2 editor scaffold.

## Clone and build

```powershell
git clone --recurse-submodules <repository-url> PHOQER
cd PHOQER
powershell -ExecutionPolicy Bypass -File Tools/BootstrapIPlug2.ps1
cmake -S . -B Build-iPlug2 -G "Visual Studio 17 2022" -A x64
cmake --build Build-iPlug2 --config Release
```

The migration targets the Windows Standalone app, VST3, and CLAP. iPlug2 is pinned as `vendor/iPlug2`; the bootstrap script retrieves the upstream VST3 and CLAP SDKs expected by iPlug2 without committing nested generated dependency trees.

## Architecture

The core defines three independent seal-character slots: `Low / Burping`, `Main / Bark-Groan`, and `Pad / Moan-Shout`. A temporary three-way selector exposes the architecture while the final editor and themes are still pending. Only the middle `Main / Bark-Groan` character is currently implemented and it remains the default. The low and pad characters are intentionally silent placeholders until their reference audio and synthesis designs are approved. They do not reuse or recolor the main character.

`PhoqerEngine` owns eight fixed seal voices, sample-accurate MIDI dispatch, character selection, an optional room stage, transparent output gain, and lock-free telemetry. It has no framework dependency. The implemented `Main / Bark-Groan` character produces a finite fast bark-to-groan call from a clean band-limited glottal source with an extended phase-locked harmonic ladder, a falling attack followed by rising decay/release contours, a sustained open body, a linear throat stage, and a four-way parallel formant bank. Register compensation lowers and broadens the tract while adding chest and harmonic energy below the tenor range, leaving the established upper register unchanged. Chest-pressure thrusts shape articulation without cyclic pitch modulation. Per-note variation is restricted to breath and articulation; pitch and tract tuning are deterministic. The optional detuned second glottal source is silent at the default zero setting. The loudest or most recent active voice drives face state.

iPlug2 owns host format integration, parameter serialization, MIDI translation, and the editor. Stable parameters are `boom`, `air`, `bark`, `vowel`, `reverb` (internally retaining the stable `space` ID), `tide`, `detune`, `output`, and the three-state `character` selector. The seven macros are normalized; output spans -24 to +18 dB.

Face telemetry is published as normalized atomics and may be polled by a future editor at 30-60 Hz. The output waveform uses a fixed single-writer/single-reader ring; meter peak and RMS are atomic.

Not implemented: behaviour profiles beyond CALL plus its BARK transient, presets, installers, project licensing, and release automation.

## Framework licensing

iPlug2 uses its permissive zlib-style license. VST3 and CLAP remain under their own upstream licenses. PHOQER's own project license is intentionally not selected by this migration.
