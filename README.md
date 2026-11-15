<p align="center">
  <img src="Media/PHOQER_Logo.png" width="340" alt="PHOQER">
</p>

<p align="center">
  <img src="Media/PHOQER_Cast.png" width="760" alt="BURP, GROAN and SQUEAL singing side by side">
</p>

<p align="center"><strong>A seal you can play. A synthetic vocal instrument for Windows and macOS.</strong></p>

<p align="center">
  <a href="https://github.com/lew0nn/phoqer/releases/tag/v1.1.0"><img alt="Version 1.1.0" src="https://img.shields.io/badge/version-1.1.0-FF9F1C?style=flat-square&amp;labelColor=322412"></a>
  <a href="#download"><img alt="Windows and macOS" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-FF792F?style=flat-square&amp;labelColor=321F15"></a>
  <a href="#download"><img alt="VST3, AU and standalone" src="https://img.shields.io/badge/formats-VST3%20%7C%20AU%20%7C%20Standalone-FF5342?style=flat-square&amp;labelColor=321A17"></a>
  <a href="https://github.com/juce-framework/JUCE/releases/tag/8.0.8"><img alt="JUCE 8.0.8" src="https://img.shields.io/badge/JUCE-8.0.8-FF2D55?style=flat-square&amp;labelColor=32141A"></a>
  <a href="CMakeLists.txt"><img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-E5378E?style=flat-square&amp;labelColor=2E1622"></a>
  <a href="LICENSE"><img alt="GNU AGPL v3" src="https://img.shields.io/badge/license-AGPL%203.0-CA41C6?style=flat-square&amp;labelColor=2A172A&amp;logo=gnu&amp;logoColor=white"></a>
  <a href="https://github.com/lew0nn/phoqer/releases/latest"><img alt="Downloads" src="https://img.shields.io/github/downloads/lew0nn/phoqer/total?style=flat-square&amp;label=downloads&amp;labelColor=271832&amp;color=B04BFF&amp;logo=github&amp;logoColor=white"></a>
</p>

<br>
<br>

PHOQER builds every call while you play it. Formant-wave-function synthesis, the technique behind
Chant and Delay Lama, makes each resonance of the seal's throat from overlapping grains, shaped by
formant tables and pitch contours measured from real seal calls. The recordings were the reference;
none of them are in the instrument.

<br>

## Features

- **Three seals:** BURP, SQUEAL and GROAN, each with its own voice, colours and a portrait that
  reacts as it sings. A fourth, [KYRAX](#kyrax), is sold separately.
- **Five moods** for a held note: one call, honks, barks, a wail or a hum, in time with your song.
- **Nine knobs**, from BOOM to GLIDE, every one automatable.
- **Eight voices**, with velocity, pitch bend, the sustain pedal and the mod wheel.
- **No samples:** every call is synthesised while you play.
- Ships with **16 presets**.
- **Standalone app** with a computer-key piano, REC to WAV and a metronome with a count-in.
- **VST3, AU and standalone** for Windows and macOS. Free and open source.

<br>
<br>

## Download

### [Windows installer](https://github.com/lew0nn/phoqer/releases/download/v1.1.0/PHOQER-1.1.0-Windows-x64-Setup.exe)

Installs the VST3 plug-in, the standalone app, or both.

### [MSI installer](https://github.com/lew0nn/phoqer/releases/download/v1.1.0/PHOQER-1.1.0-Windows-x64.msi)

Installs both in one step, for managed or scripted installs.

### [VST3 for Windows](https://github.com/lew0nn/phoqer/releases/download/v1.1.0/PHOQER-1.1.0-Windows-x64-VST3.zip)

The complete `PHOQER.vst3` bundle for Windows x64 hosts.

### [Standalone for Windows](https://github.com/lew0nn/phoqer/releases/download/v1.1.0/PHOQER-1.1.0-Windows-x64-Standalone.exe)

One file that runs PHOQER without a DAW; audio and MIDI devices are set from its EDIT menu.

### [macOS installer](https://github.com/lew0nn/phoqer/releases/download/v1.1.0/PHOQER-1.1.0-macOS.dmg)

One installer for the AU, the VST3 and the standalone app; Customize lets you leave any of them out.
Universal (Apple Silicon and Intel), macOS 10.13 or later.

### [Source](https://github.com/lew0nn/phoqer/releases/download/v1.1.0/PHOQER-1.1.0-Source.zip)

The source code with JUCE included, ready to build.

The installers and the app are not code-signed, so Windows SmartScreen may stop them the first time: choose
**More info**, then **Run anyway**. The Mac installer isn't notarized by Apple: if macOS won't open it, allow it
under System Settings, Privacy & Security, then rescan plug-ins in your host.

<br>
<br>

## Seals

<p align="center">
  <img src="Media/PHOQER_Seals.gif" width="100%" alt="BURP, SQUEAL and GROAN calling together">
</p>

- **BURP:** the bass, and the default. A low, rough bark whose growl comes from jitter and period
  doubling, not breath.
- **SQUEAL:** short, bright yelps that start high, drop, then lift again.
- **GROAN:** a long, warm "ooh" that scoops up into the note and sways with a slow vibrato.

Each seal has its own colours, and its portrait reacts as it sings.

<br>

## KYRAX

KYRAX is a fourth seal, sold separately for $5 at [lewonn.itch.io/kyrax](https://lewonn.itch.io/kyrax). It plays
with all of PHOQER's moods, knobs and presets, and adds three things of its own:

- **NITRO** turns one key into a chord: a fifth and an octave, or notes you pick, spread wide or strummed.
- **ARP** plays what you hold as an arpeggio, locked to your song's beat, with 16 steps you can draw.
- **YAP** makes KYRAX talk: its mouth moves through vowels in time with your song.

One key unlocks KYRAX on 3 of your computers. It needs the internet once, then plays offline for up to 14 days at
a time. KYRAX's voice comes with the key, so it isn't in this repository or in the downloads; everything else in
PHOQER stays free and open source.

<br>

## Moods

How a held note behaves. HONK, BARK and WAIL keep time with your song.

| Mood | What the seal does |
| --- | --- |
| CALL | One call per note |
| HONK | Nasal honks on 1/8 notes for as long as you hold the key |
| BARK | Hard barks on 1/4 notes, silence between |
| WAIL | One long glide up and back over two bars |
| MURMUR | A muttered, closed-mouth hum |

<br>

## Controls

| Knob | What it does |
| --- | --- |
| BOOM | The seal's size: small, or big with more chest |
| AIR | Breath, from none at all |
| BARK | Attack and growl: a soft swell to a hard hit |
| SPACE | Reverb: dry to a big dark cave |
| VOWEL | OO, OH, AH, EH, EE |
| DETUNE | Width: one voice to a wide double |
| TIDE | Expression: a straight note to scoops and vibrato |
| GLIDE | 0 to 2 s: a note played over a held one slides in from its pitch; chords struck together don't slide |
| OUTPUT | The level. A limiter holds the ceiling, so push it |

MIDI: velocity, pitch bend (two semitones), the sustain pedal (CC 64), and the mod wheel (CC 1),
which adds TIDE on top of the knob. Every knob can be automated.

<br>

## Standalone app

The standalone app adds a few things of its own:

- **KEYS.EXE** turns the computer keyboard into a piano: A to ; play, Z / X change octave,
  C / V change velocity.
- **REC** records what you play to a WAV file.
- **A metronome** beside REC. Click the BPM to open METRO.EXE: tempo, volume, 2/4 to 7/4 with an
  accented first beat, three click sounds (BEEP, WOOD and SEAL), and a one- or two-bar count-in
  before REC starts. The click is never recorded.

In a DAW, PHOQER follows the project's tempo, and the metronome and REC are hidden.

<p align="center">
  <img src="Media/PHOQER_Standalone.png" width="100%" alt="The standalone app as GROAN, with METRO.EXE open">
</p>

<br>

## Interface

Ships with 16 presets; save your own as `.phqpreset` files. The interface is a set of Win98 programs: the seal's portrait,
SCOPE.EXE with its waveform and spectrogram, LEVEL.EXE, MIXER.EXE and KEYS.EXE. The menus hold undo
and redo, copying and pasting settings, reverting to the preset, a panic button, zoom, a controls
guide and the keyboard shortcuts.

<br>

## Technical details

<details>
<summary><strong>Sound engine & level</strong></summary>

Each seal is a table of formant frequencies, bandwidths and amplitudes, plus the pitch contour of a
real call: where it starts, how it scoops and where it settles. Eight voices share one engine; a held
note sounds for as long as the key is down, and the mood keeps moving it.

One note at full velocity peaks near -10 dBFS. A stereo-linked lookahead limiter holds the output at
-0.3 dBFS; it looks 2 ms ahead, which PHOQER reports to the host as latency.

</details>

<details>
<summary><strong>Presets & state</strong></summary>

Host state uses stable parameter IDs, so sessions saved with earlier versions load, with any newer
setting at its default. Presets carry the sound only: loading one never changes OUTPUT, the tempo or
the metronome, and older presets set GLIDE back to 0. The standalone app's tempo and metronome
settings are not automatable.

</details>

<details>
<summary><strong>CPU cost</strong></summary>

PHOQER's cost follows how many notes are sounding: each held note is its own voice, built from
grains in real time. Eight held notes, the most it plays at once, take 4 to 5% of one core.

Measured on one core of an Intel Core i5-14400F, 44.1 kHz, stereo, 128-sample blocks, one instance
with its default settings, as a share of realtime (the sound engine; the window's drawing is not
included):

| Notes held | BURP | SQUEAL | GROAN |
| --- | --- | --- | --- |
| 1 | ~1% | ~1% | ~1% |
| 4 | ~2.5% | ~2.5% | ~2.5% |
| 8 | ~4.5% | ~4% | ~4.5% |

KYRAX, measured through the whole plug-in on the same machine:

| Notes held | KYRAX | with NITRO | with ARP | with YAP |
| --- | --- | --- | --- | --- |
| 1 | ~1% | ~2.5% | ~1.5% | ~1% |
| 4 | ~3% | ~8% | ~1.5% | ~3% |
| 8 | ~6% | ~11% | ~1.5% | ~6% |

NITRO plays up to four notes for every key, so it costs the most. ARP plays one note at a time, however
many keys you hold.

To measure your own machine, build from source and run `PHOQERPerformanceSanity --cpu`.

</details>

<details>
<summary><strong>Testing</strong></summary>

Steinberg's VST3 validator passes all 47 tests. PHOQER's own checks cover the engine and the three
seals, chords and GLIDE, the limiter's ceiling across sample rates and block sizes, session recall
and presets, the metronome's timing and count-in, and the built VST3 in a host across 300
combinations of seal, mood, sample rate and block size. See [README_DEV.md](README_DEV.md).

</details>

<details>
<summary><strong>Manual installation</strong></summary>

### VST3

Extract the VST3 package and copy the complete `PHOQER.vst3` bundle to:

```text
C:\Program Files\Common Files\VST3
```

Rescan plug-ins in the host.

### Standalone

Run `PHOQER-1.1.0-Windows-x64-Standalone.exe` from anywhere. Audio and MIDI are set from EDIT ▸ AUDIO/MIDI SETUP.

### macOS

Open `PHOQER-1.1.0-macOS.dmg` and run `Install PHOQER.pkg`. It puts `PHOQER.app` in Applications and the AU and
VST3 in `/Library/Audio/Plug-Ins`. If macOS says it can't check the installer, open System Settings, Privacy &
Security, scroll down and press Open Anyway. Then rescan plug-ins in your host.

</details>

<details>
<summary><strong>Build from source</strong></summary>

Requirements: Windows x64, CMake 3.22+, Visual Studio 2022 with the MSVC desktop C++ toolchain, Git,
and the pinned JUCE submodule (or the Source zip from a release, which includes JUCE). For the
installers, [Inno Setup 6](https://jrsoftware.org/isinfo.php) and [WiX Toolset 3.14](https://github.com/wixtoolset/wix3/releases).

```powershell
git clone --recurse-submodules https://github.com/lew0nn/phoqer.git
Set-Location phoqer
powershell -ExecutionPolicy Bypass -File .\build-release.ps1
```

Every release file is written to `dist\release`. Without Inno Setup or WiX the script skips that
installer and builds the rest.

On macOS, with Xcode's command-line tools and CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target PHOQER_AU PHOQER_VST3 PHOQER_Standalone
```

The Mac release files come from the `macOS build` GitHub Action.

Built with C++17, JUCE 8.0.8 and CMake.

</details>

<br>

## License

<p align="center">
  <strong><a href="LICENSE">GNU Affero General Public License v3.0</a></strong>
</p>

<p align="center">PHOQER's original source code and project assets are free and open-source software.<br>
Make and release music with it however you like: no credit needed, no royalties.</p>

<p align="center">JUCE and all other third-party components retain their respective licences.<br>See <a href="THIRD_PARTY_NOTICES.md">Third-Party Notices</a>.</p>

<p align="center"><sub>Copyright © 2026 lewonn / LWNX DSP. VST is a trademark of Steinberg Media Technologies GmbH.</sub></p>

<p align="center">Bugs and sound ideas: <a href="https://github.com/lew0nn/phoqer/issues">Issues</a> · <a href="CONTRIBUTING.md">Contributing</a></p>

<p align="center"><strong>PHOQER 1.1.0 · LWNX DSP</strong></p>
