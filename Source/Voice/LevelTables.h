#pragma once

namespace phoqer::levels
{
// Each mode's level against CALL, in dB, per voice (BURP, SQUEAL, GROAN, the loaded voice): HONK's
// short hits and MURMUR's closed mouth came out 6 to 10 dB under CALL. Measured on the loudest 400 ms
// (BS.1770 K-weighted, notes 48/55/62 at the defaults) by the plugin's own engine.
// Tests/Levels.cpp (PHOQERLevels) measures them again and checks them; --write rewrites the built-in rows.
inline constexpr float modeTrimDb[4][5] {
    // CALL  HONK   BARK  WAIL  MURMUR
    { 0.0f,   7.9f,  1.4f,  1.2f,  5.7f },   // BURP
    { 0.0f,   6.9f,  2.8f,  1.7f,  6.4f },   // SQUEAL
    { 0.0f,   8.9f,  4.9f,  0.1f,  0.4f },   // GROAN
    { 0.0f,  6.2f, 0.1f, 1.0f, 8.7f } };   // loaded voice (KYRAX), measured with phoqer-step-lab/kyraxlevels the same way
}
