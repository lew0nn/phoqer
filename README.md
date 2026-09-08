# PHOQER

A synthetic MIDI instrument built around seal-like barks, groans, and vocal calls.

PHOQER generates its sound in real time. Recorded calls are sound-design references;
the instrument does not play back seal samples.

Built with C++17 and JUCE 8.0.8. Current targets are VST3 and Standalone for Windows x64.

## Development status

PHOQER is an early work in progress. Its engine and functional interface are still being
refined before the first public release.

Three sound characters are present:

- **Main / Bark-Groan:** the working engine and default selection.
- **Low / Burp:** reserved for the upcoming low character and currently silent.
- **Pad / Moan-Shout:** reserved for the upcoming pad character and currently silent.

The implemented Main character has eight-voice polyphony, harmonic and vowel shaping,
pitch movement, optional detune, reverb, behavior profiles, and output gain. Each sound
character owns one visual identity: Default is Purple, Burp is Red, and Synth is Ice.
The editor also includes a live waveform and output meter.

## Controls

BOOM, AIR, BARK, SPACE, and TIDE are exposed in the main interface. VOWEL, DETUNE,
OUTPUT, CHARACTER, and BEHAVIOR are also stable host parameters. Detune and
reverb start at zero. Begin with a low monitoring level when increasing output gain.

## Build from source

Requirements: Git, CMake 3.22 or newer, Visual Studio 2022 with the **Desktop development
with C++** workload, and a Windows SDK.

```powershell
git clone --recurse-submodules https://github.com/lew0nn/phoqer.git
Set-Location phoqer
cmake -S . -B Build-JUCE -G "Visual Studio 17 2022" -A x64
cmake --build Build-JUCE --config Release --target PHOQER_VST3 PHOQER_Standalone
```

The results are written below `Build-JUCE/PHOQER_artefacts/Release/`. Copy the complete
`PHOQER.vst3` bundle to a VST3 directory scanned by your host, then rescan plug-ins.

The repository path should not contain parentheses because JUCE's Windows binary-data
generator is passed through MSBuild command files.

For engine details, see [README_DEV.md](README_DEV.md).

## License

Original PHOQER code, documentation, and project assets are licensed under
[GNU AGPL v3](LICENSE), `AGPL-3.0-only`, copyright 2026 lewonn / LWNX DSP.

You may use PHOQER to make and release music without crediting the author or paying
royalties. If you distribute PHOQER itself or a modified version, the AGPL's source,
license, and notice requirements apply.

JUCE, the VST3 SDK, Roboto, and other third-party components remain under their own
licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the notices retained
inside `vendor/JUCE`.

VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and
other countries.

## Feedback and contributions

Bug reports and sound-design feedback are welcome in
[Issues](https://github.com/lew0nn/phoqer/issues). For audio problems, include your host,
sample rate, note range, and settings. Read [CONTRIBUTING.md](CONTRIBUTING.md) before
submitting code or assets.
