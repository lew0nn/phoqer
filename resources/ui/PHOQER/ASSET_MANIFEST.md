# Authoritative PHOQER production asset manifest

All crop bounds are x, y, width, height in native source pixels. Originals are preserved in Sources and never embedded. Offline cleanup clones original local material; no generated artwork. Runtime embeds only the production derivatives.

| Output | Source sheet | Crop bounds | Purpose / cleanup |
|---|---|---|---|
| Knobs/Purple/knob_body_purple.png | ChatGPT Image Sep 5, 2026, 04_33_55 PM (1).png | 197, 581, 423, 423 | Default knob, neutral state; radial adjacent-pixel pointer cleanup; pointer |
| Knobs/Ice/knob_body_ice.png | ChatGPT Image Sep 5, 2026, 04_33_55 PM (2).png | 23, 195, 309, 309 | Synth knob, unlit state; pointer |
| Knobs/Red/knob_body_red.png | ChatGPT Image Sep 5, 2026, 04_33_55 PM (3).png | 55, 101, 384, 384 | Burp knob, unlit state; pointer |
| Tide/Purple/tide_body_purple.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (4).png | 910, 91, 280, 909 | Empty industrial housing; remove preview column with recessed source material; purpletrack |
| Tide/Purple/tide_track_purple.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (4).png | 1013, 172, 67, 100 | Unlit recessed track texture;  |
| Tide/Purple/tide_fill_purple.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (4).png | 448, 173, 111, 733 | Luminous column, clipped at runtime;  |
| Tide/Purple/tide_handle_purple.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (4).png | 647, 444, 205, 166 | Single detached hardware handle;  |
| Tide/Ice/tide_body_ice.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (5).png | 429, 91, 244, 895 | Ice housing with baked meter column removed by adjacent recessed material; icetrack |
| Tide/Ice/tide_track_ice.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (5).png | 511, 180, 76, 100 | Unlit ice recess;  |
| Tide/Ice/tide_fill_ice.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (5).png | 511, 349, 76, 565 | Source luminous ice column;  |
| Tide/Ice/tide_handle_ice.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (5).png | 142, 190, 122, 72 | Single ice handle from slider;  |
| Tide/Red/tide_body_red.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (5).png | 1121, 91, 245, 895 | Red housing with baked meter column removed by adjacent recessed material; redtrack |
| Tide/Red/tide_track_red.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (5).png | 1204, 180, 76, 100 | Unlit red recess;  |
| Tide/Red/tide_fill_red.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (5).png | 1204, 349, 76, 565 | Source luminous red column;  |
| Tide/Red/tide_handle_red.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (5).png | 835, 190, 122, 72 | Single red handle from slider;  |
| Buttons/button_voice.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 39, 861, 105, 104 | Exact developer-map hardware sidebar button;  |
| Buttons/button_formant.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 176, 861, 105, 104 | Exact developer-map hardware sidebar button;  |
| Buttons/button_harmonics.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 313, 861, 105, 104 | Exact developer-map hardware sidebar button;  |
| Buttons/button_space.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 450, 861, 105, 104 | Exact developer-map hardware sidebar button;  |
| Buttons/button_bite.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 587, 861, 105, 104 | Exact developer-map hardware sidebar button;  |
| SmallParts/led_purple.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (6).png | 774, 63, 103, 104 | Supplied active LED;  |
| Displays/meter_off_purple.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 789, 497, 305, 60 | Housing, scale, original unlit cells replicated across strip; meter_off |
| Displays/meter_on_purple.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 789, 497, 305, 60 | Original lit cells replicated; reveal only live level; meter_on |
| Displays/wave_frame_purple.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 1149, 497, 260, 59 | Original display frame, baked trace removed with clean interior pixels; wave |
| SmallParts/led_ice.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (6).png | 774, 186, 103, 104 | Supplied active LED;  |
| Displays/meter_off_ice.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 789, 585, 305, 60 | Housing, scale, original unlit cells replicated across strip; meter_off |
| Displays/meter_on_ice.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 789, 585, 305, 60 | Original lit cells replicated; reveal only live level; meter_on |
| Displays/wave_frame_ice.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 1149, 585, 260, 59 | Original display frame, baked trace removed with clean interior pixels; wave |
| SmallParts/led_red.png | ChatGPT Image Sep 5, 2026, 04_33_56 PM (6).png | 774, 309, 103, 104 | Supplied active LED;  |
| Displays/meter_off_red.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 789, 675, 305, 60 | Housing, scale, original unlit cells replicated across strip; meter_off |
| Displays/meter_on_red.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 789, 675, 305, 60 | Original lit cells replicated; reveal only live level; meter_on |
| Displays/wave_frame_red.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 1149, 675, 260, 59 | Original display frame, baked trace removed with clean interior pixels; wave |
| SmallParts/led_off.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 1080, 861, 36, 37 | Supplied unlit LED;  |
| SmallParts/menu_icon.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 1210, 857, 53, 47 | Supplied menu hardware;  |
| SmallParts/vent_grille.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 1310, 853, 94, 59 | Supplied vent, reserved;  |
| SmallParts/screw.png | ChatGPT Image Sep 5, 2026, 04_34_38 PM.png | 1341, 964, 40, 42 | Supplied screw, reserved;  |


Approved seal resources remain ../Purple/purple_seal.png, ../Red/red_seal.png, ../Ice/ice_seal.png unchanged. Their source frame is the sole portrait frame.
The common developer-map buttons retain their original material colors. The selected LED follows the sound identity.
Meter derivatives share unchanged housing/scale; runtime clips the lit derivative to whole native segment cells. Wave-frame interiors contain no baked waveform.
