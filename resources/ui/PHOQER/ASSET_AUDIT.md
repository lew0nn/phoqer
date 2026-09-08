# PHOQER asset audit (before UI modifications)

| Element | Current resource/rendering | Authoritative source | Action |
|---|---|---|---|
| Four knob bodies | Shared knob sheets; runtime pointer cleanup plus procedural sheen | ZIP sheets 1, 2, 3, neutral states | Extract native-resolution bodies; clean pointer offline; remove sheen |
| Knob pointer | Long line reaching outer ring | One dynamic pointer inside metallic cap | Shorten and move inward |
| TIDE | Neutralized purple sheet housing; procedural track/fill/thumb | ZIP sheets 4 and 5 | Extract individual housing, track, luminous fill, handle |
| Sidebar | Old Purple/Ice/Red mode PNGs plus selection border | Developer map, five button bodies | Extract exact bodies; LED selection without extra border |
| Seal | Approved purple_seal, red_seal, ice_seal plus outer procedural display | Same approved assets, including their existing frame | Keep exact portraits, remove outer frame; near-square module |
| Waveform | 900ms rolling min/max, faint fill | Real telemetry; developer-map display housing/style | Extract empty housing, strengthen filled shape; retain whole event |
| Output meter | Procedural segments and frame | Three meter strips in developer map | Extract housing/scale and lit/unlit segment derivatives |
| Menu | Three procedural strokes | Developer map menu_icon | Extract supplied menu |
| LEDs/vent/screw | Unused shared sheets | Developer map small parts | Extract useful supplied parts; cache LEDs/menu |
| Shell/header/labels | Existing JUCE layout/text | No replacement shell supplied by this ZIP | Retain shell and labels; compact content geometry |

All existing UI resources were enumerated from resources/ui; CMake embeds only a subset. Source sheets are preserved; runtime will embed extracted PNGs only, approved seals and the existing font. No synthesis change is authorized in this pass.
