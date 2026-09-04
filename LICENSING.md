# PHOQER licensing

PHOQER's original project material uses the unmodified BSD-3-Clause license in
[LICENSE](LICENSE). SPDX identifier: `BSD-3-Clause`.

## Scope and history

The license covers original PHOQER code in `Source/`, build scripts,
`config.h`, documentation, resource definitions, and original project assets
owned by lewonn / LWNX DSP, unless separately identified. Accepted contributions
remain owned by their authors under the terms in [CONTRIBUTING.md](CONTRIBUTING.md).

Third-party material is excluded. In particular, `vendor/iPlug2`, downloaded
format SDKs, and `resources/fonts/Roboto-Regular.ttf` retain their own
licenses. The bundled Roboto font uses Apache-2.0. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

lewonn / LWNX DSP also offers original PHOQER material they own in earlier
repository revisions under BSD-3-Clause. This additional permission does not
withdraw rights already granted under earlier licenses or alter third-party
terms. Historical JUCE references remain subject to JUCE's applicable license.
The Git history is preserved.

## Author credit and commercial use

Both commercial and noncommercial development and distribution are permitted,
including closed-source derivatives, without contacting the author.

Source redistributions retain the copyright notice, conditions, and disclaimer.
Binary redistributions reproduce them in accompanying documentation or other
supplied materials. The author credit is the copyright notice naming
lewonn / LWNX DSP in LICENSE. A separate UI credit or advertising acknowledgment
is not required. Private changes do not require a public acknowledgment.

Musicians and consumers owe no author credit or royalties merely for using
PHOQER or distributing their own audio output. Anyone redistributing the
software itself must retain its notices. This explanation adds no conditions
to BSD-3-Clause; see [COMMERCIAL.md](COMMERCIAL.md) for examples.

## Contributions

Contributors retain their copyright and offer accepted original material
under BSD-3-Clause. No separate commercial relicensing agreement or CLA is
needed for this model. Keep their copyright and license notices.

## Distribution

Include LICENSE, COMMERCIAL.md, THIRD_PARTY_NOTICES.md, and LICENSES/ in
PHOQER's official binary packages. Preserve the notices in any third-party
source you distribute and review dependency license changes when updating
the pinned framework or SDKs.

The build collects notices in `out/PHOQER-Licenses` and places them inside
`PHOQER.vst3/Contents/Resources/PHOQER-Licenses`. A CLAP package must include
the collected folder beside the binary. Source packages include the root
license and dependency notices; initialize the pinned submodule and use
`Tools/BootstrapIPlug2.ps1` to obtain build dependencies.

The Windows standalone target remains disabled pending resolution of the ASIO
licensing issue. BSD-3-Clause on PHOQER does not grant rights over the bundled
legacy ASIO files. Current build targets are VST3 and CLAP.

Official BSD-3-Clause terms:
https://opensource.org/license/bsd-3-clause

The dependency review and its limits are recorded in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md#bsd-3-clause-compatibility-review).
