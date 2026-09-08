# Third-party notices

PHOQER's root `LICENSE` applies to original PHOQER material owned by lewonn / LWNX DSP.
It does not replace or remove third-party licenses.

## JUCE 8.0.8

PHOQER uses JUCE at commit `d6181bde38d858c283c3b7bf699ce6340c050b5d` (tag 8.0.8).
JUCE modules are dual-licensed under AGPLv3 and the commercial JUCE licence; this project
uses them under AGPLv3. The complete upstream source and notices are retained in
`vendor/JUCE`. See `vendor/JUCE/LICENSE.md`.

JUCE contains third-party components under their own licenses. The authoritative list
and notice paths are in `vendor/JUCE/LICENSE.md`. The Windows targets used here include
JUCE's VST3 SDK, graphics, font-shaping, image, ZIP, and audio support dependencies as
applicable to the selected modules.

## Steinberg VST3 SDK

The VST3 SDK bundled by JUCE is available under the Steinberg VST3 License or GPLv3.
PHOQER uses the GPLv3-compatible option alongside AGPLv3. Its license texts are retained
under `vendor/JUCE/modules/juce_audio_processors/format_types/VST3_SDK/`.

VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other
countries. Compatibility wording does not imply endorsement.

## Roboto

The embedded Roboto Regular font is provided under Apache License 2.0. Its upstream
license and notice are retained in `LICENSES/Apache-2.0.txt` and
`LICENSES/Roboto-NOTICE.txt`.

## Distribution

Binary distributions must include the PHOQER AGPL license, this notice, and all notices
required by the corresponding JUCE and embedded third-party source. Source distributions
must keep the pinned JUCE source and its notices intact. No third-party component is
relicensed as PHOQER-owned material.
