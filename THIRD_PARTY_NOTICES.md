# Third-party notices

PHOQER's BSD-3-Clause root `LICENSE` applies only to original PHOQER material owned
by lewonn / LWNX DSP. It does not replace, sublicense, or add restrictions to
the third-party software listed below. Copyright and trademark rights in those
components remain with their respective owners.

The corresponding license texts used for distribution are copied in
`LICENSES/`. License files and notices embedded in upstream source files also
remain controlling and must not be removed.

## Current framework and format dependencies

| Component | Revision used | License | Local notice |
| --- | --- | --- | --- |
| iPlug2, including its iPlug, IGraphics, and WDL code used by PHOQER | `d54f69050f517e43b941d88c2a170f0a840b9ee4` | zlib-style; included components retain their listed licenses | `LICENSES/iPlug2.txt` |
| Steinberg VST 3 SDK | `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` | MIT | `LICENSES/VST3-SDK.txt` |
| CLAP SDK | `a47f6badb49d948fd009998f28309cdab78979c9` | MIT | `LICENSES/CLAP-SDK.txt` |
| CLAP helpers | `c35dd4906bd8efbb900cb2b89e680fed463cc8b1` | MIT | `LICENSES/CLAP-Helpers.txt` |
| NanoVG | version contained in the pinned iPlug2 revision | zlib | `LICENSES/NanoVG.txt` |
| NanoSVG | version contained in the pinned iPlug2 revision | zlib | `LICENSES/NanoSVG.txt` |
| FontStash | version contained in NanoVG in the pinned iPlug2 revision | zlib | `LICENSES/FontStash.txt` |
| stb_textedit (and stb_image_write, when used) | version contained in the pinned iPlug2 revision | MIT or public domain, at the recipient's option | `LICENSES/STB.txt` and notices embedded in the relevant headers |
| stb_image 2.10 and stb_truetype 1.09 | older headers inside NanoVG | public-domain declarations in these versions | headers in `vendor/iPlug2/Dependencies/IGraphics/NanoVG/src/`; the later MIT text must not be assumed to describe these versions |
| glad-generated OpenGL loader and Khronos platform declarations | version contained in the pinned iPlug2 revision | generated glad code is offered as public domain/WTFPL/CC0; Khronos declarations retain their embedded notice | `LICENSES/Khronos-khrplatform.txt` and notices embedded in the relevant files |
| Roboto Regular 1.100141 (2013) | NanoVG example font; font data copyright Google 2012 | Apache License 2.0 | `LICENSES/Roboto-NOTICE.txt` and `LICENSES/Apache-2.0.txt` |

VST® is a trademark of Steinberg Media Technologies GmbH, registered in Europe
and other countries. PHOQER uses the term only to describe compatibility and
does not claim ownership of the mark or endorsement by Steinberg.

The iPlug2 license inventory also identifies licenses for components present in
its source tree but not necessarily compiled into PHOQER. A full source
distribution must preserve the complete upstream trees and every notice they
contain, not only the summary above.

Official upstream references:

- iPlug2: https://github.com/iPlug2/iPlug2/tree/d54f69050f517e43b941d88c2a170f0a840b9ee4
- VST 3 SDK: https://github.com/steinbergmedia/vst3sdk/tree/3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
- CLAP SDK: https://github.com/free-audio/clap/tree/a47f6badb49d948fd009998f28309cdab78979c9
- CLAP helpers: https://github.com/free-audio/clap-helpers/tree/c35dd4906bd8efbb900cb2b89e680fed463cc8b1
- NanoVG: https://github.com/memononen/nanovg
- NanoSVG: https://github.com/memononen/nanosvg
- stb: https://github.com/nothings/stb
- glad: https://github.com/Dav1dde/glad
- Steinberg VST trademark guidance: https://github.com/steinbergmedia/vst3sdk#trademark-and-logo-usage

## ASIO and standalone builds

The pinned iPlug2 source tree contains RtAudio, RtMidi, and legacy Steinberg
ASIO interface files used by iPlug2's default Windows standalone-app target.
They are not used by PHOQER's VST3 or CLAP plug-in targets.

PHOQER's current build configuration omits the standalone target pending
resolution of the separate ASIO licensing issue. Distributors enabling that
target must remove/replace the ASIO path or establish and comply with a license
applicable to the ASIO files they use. PHOQER's BSD-3-Clause license does not
grant rights over Steinberg's material.

Official ASIO licensing information:
https://www.steinberg.net/developers/asiosdk-open/

RtAudio and RtMidi themselves use permissive MIT-like licenses, retained in
their upstream directories. That does not determine the separate licensing of
the Steinberg ASIO files.

## Historical JUCE reference

The initial repository revision referenced JUCE as a Git submodule at revision
`d6181bde38d858c283c3b7bf699ce6340c050b5d`. JUCE is not part of the current
PHOQER build. The historical gitlink does not make JUCE PHOQER-owned material,
and neither the current nor any earlier PHOQER license grants rights to JUCE.
Anyone retrieving third-party content from repository history must comply with
the license applicable to that content.

## Distribution rule

Every binary package must include the root `LICENSE`, this file, and the full
`LICENSES/` directory. A source package must additionally retain all original
copyright and license notices in every included dependency. Do not make a
manually flattened or stripped archive of `vendor/iPlug2`.

No third-party component is relicensed by PHOQER.

## BSD-3-Clause compatibility review

For the current Windows NanoVG/GL2 VST3 and CLAP targets, the source-level
licenses reviewed permit combination with BSD-3-Clause PHOQER code, subject to
retaining each dependency's own notices. iPlug2's three restrictions concern
origin, marking altered upstream source, and preserving notices. MIT requires
its copyright and permission notice; Apache-2.0 section 4 governs redistribution
of the bundled Roboto font. These terms are retained separately from PHOQER's
BSD-3-Clause license.

The review used the pinned sources, generated project files, and the Windows
compiler's dependency logs, including graphics headers compiled through
IGraphicsWin.cpp. Those logs include NanoVG, FontStash, NanoSVG, glad/Khronos,
stb, WDL, and the selected format SDK. They contain no ASIO, RtAudio, RtMidi,
JUCE, Skia, Yoga, or WebView source in the two plug-in targets. Optional
components present elsewhere in the framework are not covered by this binary
review; changing formats, graphics backends, or dependencies needs a new check.

The iPlug2 working tree was unmodified at review. The bundled Roboto file is
unchanged from NanoVG's example and identifies Apache-2.0 in its metadata.
No conclusion about compliance of previously distributed binaries is implied.
