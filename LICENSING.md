# Licensing and release checklist

This file records the practical rules for maintaining PHOQER's licensing
boundary. It is not a substitute for legal advice or the controlling license
texts.

## Ownership map

- `Source/`, `config.h`, original project documentation, original resource
  definitions, and original project artwork are PHOQER Project Material under
  the root `LICENSE`, unless a file says otherwise.
- `vendor/iPlug2` is a third-party git submodule and is never covered by the
  root license.
- format SDK directories populated by `Tools/BootstrapIPlug2.ps1` remain under
  their upstream licenses.
- `resources/fonts/Roboto-Regular.ttf` is under the Apache License 2.0; retain
  its copyright notice and the complete license text.
- files carrying their own copyright or license notice remain under that
  notice.

## Before publishing source

1. Confirm the iPlug2 gitlink and all bootstrapped SDK revisions match
   `THIRD_PARTY_NOTICES.md`.
2. Keep the dependency as a gitlink. Do not flatten or copy the complete
   `vendor/iPlug2` tree into a source archive.
3. Keep the root `LICENSE`, `COMMERCIAL.md`, `THIRD_PARTY_NOTICES.md`,
   `CONTRIBUTING.md`, and `LICENSES/` in the source release.
4. Do not merge external copyrightable contributions without a separate
   written contributor agreement.

GitHub's automatically generated source archives contain the superproject and
the submodule pointer, not a complete recursive dependency checkout. Build
instructions should direct developers to initialize the submodule and run the
bootstrap script.

## Before publishing binaries

1. Build only the VST3 and/or CLAP targets covered by the present audit.
2. Do not publish the iPlug2 Windows standalone target while its default ASIO
   path remains enabled.
3. Include `LICENSE`, `COMMERCIAL.md`, `THIRD_PARTY_NOTICES.md`, and the entire
   `LICENSES/` directory in every downloadable binary archive or installer.
4. Do not remove copyright or license notices from source supplied with a
   binary.
5. Re-run the dependency audit whenever a submodule or SDK revision changes.
6. Keep the Steinberg VST trademark attribution from
   `THIRD_PARTY_NOTICES.md` anywhere the product documentation refers to VST
   compatibility, and do not put VST in the PHOQER product or company name.

The build copies the distribution notices to `out/PHOQER-Licenses`. The VST3
target also embeds that directory at
`PHOQER.vst3/Contents/Resources/PHOQER-Licenses`. A CLAP zip or installer must
place the copied directory beside the `.clap` binary.
