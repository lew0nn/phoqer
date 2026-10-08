#pragma once

namespace phoqer
{
// The secret fish, 20 x 10 pixels: X outline, o body, s belly, d fins and gill, W eye, K pupil.
// The UI draws it; fed to the seal, the engine sings it (see FishSong).
inline constexpr int fishWidth = 20, fishHeight = 10;
inline constexpr const char* fishStraight[fishHeight] {
    "......XXXXX.........", "....XXdddddXX.......", "...XoooooooooXX...XX", "..XoWKodoooooooX.XdX",
    ".XooKKoodoooooooXddX", "XdooooodoooooooooddX", ".XssssdssssssssoXddX", "..XssssssssssssX.XdX",
    "...XXsssssssXXX...XX", ".....XXXXXXX........" };
// The same fish with its tail flicked.
inline constexpr const char* fishFlicked[fishHeight] {
    "......XXXXX.........", "....XXdddddXX.......", "...XoooooooooXX...XX", "..XoWKodoooooooX.XdX",
    ".XooKKoodoooooooXddX", "XdooooodoooooooooddX", ".XssssdssssssssoXdX.", "..XssssssssssssXXX..",
    "...XXsssssssXXX.....", ".....XXXXXXX........" };
}
