# PHOQER licensing guide

This is a plain-language guide. The root `LICENSE` is the controlling legal
text. PHOQER is source-available, not OSI-approved open-source software.

## No permission, credit, or payment required

You may use PHOQER as an instrument or audio tool to make music, recordings,
performances, videos, games, streams, broadcasts, sound design, and presets.
Those outputs may be released and monetized commercially. PHOQER does not
require producer credits, track attribution, royalties, or contact for normal
end-user use.

You may rebuild or privately modify PHOQER for your own music or audio
production, including commercial production, as long as source notices remain
intact. Paid development performed for an employer or client is different and
requires a commercial software license.

## Attribution required for public noncommercial forks

Public, noncommercial forks and redistributed modifications are allowed. Their
README or primary documentation must clearly state:

> Based on PHOQER by lewonn / LWNX DSP
>
> https://github.com/lew0nn/phoqer

They must also retain the project license and third-party notices, identify
their changes, and avoid implying endorsement by the original author.

## Contact and written agreement required

A separate written license is required before anyone commercially exploits
PHOQER-derived software. Examples include:

- selling or licensing PHOQER or a modified fork;
- including it in a paid software bundle or subscription;
- charging for hosted or remote access to it;
- accepting donations or sponsorship specifically for distributing or
  developing PHOQER-derived software;
- doing paid client or employer work that builds PHOQER-derived software; or
- incorporating a substantial part of PHOQER source into another commercial
  software product.

Commercial music and audio output is expressly excluded from this requirement.

To discuss commercial software licensing, contact the copyright holder through
https://github.com/lew0nn. Contact or negotiation is not permission; permission
exists only in a separate written agreement from the copyright holder.

## Third-party software

iPlug2, format SDKs, fonts, and other dependencies keep their own licenses.
PHOQER's license neither replaces nor restricts those licenses. See
`THIRD_PARTY_NOTICES.md` and `LICENSES/`.

The Windows standalone target is intentionally disabled because iPlug2's
default standalone path includes Steinberg ASIO interface material with
separate licensing requirements. VST3 and CLAP are the approved PHOQER targets
under the repository's current setup.
