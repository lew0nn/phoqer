# PHOQER

A MIDI instrument built around seal-like barks, groans, and other vocal sounds.

PHOQER generates its sound through synthesis. Recorded calls serve as references
for the sound design; the instrument does not play back seal samples.

Built with C++17 and iPlug2. Current build targets are **VST3 and CLAP for Windows x64**.

## Development status

PHOQER is an early work in progress. The sound engine is being developed first;
the current interface is temporary and the final UI will come later.

Three sound characters are planned:

- **Main / Bark-Groan:** the working sound engine and default selection.
- **Low / Burping:** planned. The current slot is silent.
- **Pad / Moan-Shout:** planned. The current slot is silent.

The main character has eight-voice polyphony, harmonic and vowel shaping,
pitch movement, optional detune, reverb, and output gain. Its sound is still
being refined. Preset management and installers are not implemented yet.

## Trying it

Load PHOQER on a MIDI track in a compatible host and select **Main / Bark-Groan**.
Play notes from a MIDI keyboard or piano roll. The Low and Pad selections do
not produce sound yet.

The current controls are BOOM, AIR, BARK, VOWEL, REVERB, TIDE, DETUNE, and OUTPUT.
Detune and reverb start at zero. Start with your monitoring level low when
experimenting with gain.

## Build from source

Requirements: Git, CMake 3.22 or newer, and Visual Studio 2022 with the
**Desktop development with C++** workload and a Windows SDK. Run these commands
from a Developer PowerShell where Git and CMake are available:

```powershell
git clone --recurse-submodules https://github.com/lew0nn/phoqer.git
Set-Location phoqer
powershell -ExecutionPolicy Bypass -File Tools/BootstrapIPlug2.ps1
cmake -S . -B Build-iPlug2 -G "Visual Studio 17 2022" -A x64 -DIPLUG_DEPLOY_PLUGINS=OFF
cmake --build Build-iPlug2 --config Release --target PHOQER-vst3 PHOQER-clap
```

The bootstrap script downloads the pinned VST3 and CLAP dependencies.
These commands leave the results in `Build-iPlug2/out/` without automatically
installing them into your host's plug-in directories:

- `PHOQER.vst3/`: copy the complete bundle into a VST3 directory scanned by your host.
- `PHOQER.clap`: copy into a CLAP directory scanned by your host, together with
  the adjacent `PHOQER-Licenses/` folder.

Rescan plug-ins in your host after copying them. The standalone target is
currently disabled pending resolution of its separate ASIO licensing requirements.

For engine details and development notes, see [README_DEV.md](README_DEV.md).

## License

Original PHOQER code and project material are licensed under
[BSD-3-Clause](LICENSE), copyright 2026 lewonn / LWNX DSP.

You can modify, redistribute, and sell derivatives, including closed-source
products. Keep the copyright notice, license conditions, and disclaimer with
redistributed source or in the materials accompanying binaries. No separate
commercial permission is required, and attribution must not imply endorsement.

Musicians and consumers do not need to credit the author or pay royalties for
using PHOQER or releasing their own audio. See [COMMERCIAL.md](COMMERCIAL.md)
for examples.

iPlug2, SDKs, fonts, and other dependencies retain their own licenses. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [LICENSES/](LICENSES/).

VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe
and other countries.

## Feedback and contributions

Bug reports and sound-design feedback are welcome in
[Issues](https://github.com/lew0nn/phoqer/issues). For audio problems, include
your host, sample rate, note range, and settings so the behavior can be reproduced.

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code or assets.
