// Mood loudness check: each mood should play as loud as CALL. Measures every built-in voice in every mood
// the way the table in Voice/LevelTables.h was first measured (48 kHz, the plugin's defaults, notes 48, 55
// and 62 held 4 s, BS.1770 K-weighting, the loudest 400 ms, energy-averaged over the three notes), with the
// table's trims applied, so a correct table measures 0 dB against CALL.
//
//   PHOQERLevels --check <LevelTables.h>    exit 0 when every mood is within 0.5 dB of CALL, 1 with a report
//   PHOQERLevels --write <LevelTables.h>    rewrite the BURP, SQUEAL and GROAN rows from this measurement
//
// The loaded voice's row (KYRAX) is left as it is: measuring it needs the voice file, which is not part of
// this source.
#include "Core/PhoqerEngine.h"
#include "Voice/LevelTables.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace phoqer;

namespace
{
constexpr int rate = 48000, block = 512;
constexpr double tolerance = 0.5;     // dB
const char* voiceNames[] { "BURP", "SQUEAL", "GROAN" };
const char* modeNames[] { "CALL", "HONK", "BARK", "WAIL", "MURMUR" };

std::vector<double> render(int voice, int mode, int note)
{
    auto engine = std::make_unique<PhoqerEngine>();
    engine->prepare(rate, block, 2);
    MacroState m;
    m.character = static_cast<SealCharacter>(voice);
    m.behaviorMode = static_cast<BehaviourMode>(mode);
    AudioBuffer buffer(2, block);
    std::vector<double> left;
    const int total = static_cast<int>(rate * 4.5), offAt = rate * 4 - (rate * 4) % block;
    for (int t = 0; t < total; t += block)
    {
        std::vector<MidiEvent> events;
        if (t == 0) events.push_back({ MidiEventType::noteOn, 0, 0, note, 0.8f });
        if (t == offAt) events.push_back({ MidiEventType::noteOff, 0, 0, note, 0.0f });
        engine->process(buffer, events.data(), static_cast<int>(events.size()), m, 0.0f);
        for (int i = 0; i < block; ++i) left.push_back(buffer.getReadPointer(0)[i]);
    }
    return left;
}

// BS.1770 K-weighting at 48 kHz (a high shelf, then a high pass), then the loudest 400 ms block (100 ms hop).
double loudest400(const std::vector<double>& x)
{
    auto biquad = [](const double b[3], const double a[3], const std::vector<double>& in)
    {
        std::vector<double> y(in.size());
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (size_t i = 0; i < in.size(); ++i)
        {
            const double v = b[0] * in[i] + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2;
            x2 = x1; x1 = in[i]; y2 = y1; y1 = v; y[i] = v;
        }
        return y;
    };
    const double shelfB[3] { 1.53512485958697, -2.69169618940638, 1.19839281085285 }, shelfA[3] { 1, -1.69065929318241, 0.73248077421585 };
    const double passB[3] { 1, -2, 1 }, passA[3] { 1, -1.99004745483398, 0.99007225036621 };
    const auto y = biquad(passB, passA, biquad(shelfB, shelfA, x));
    const int win = rate * 4 / 10, hop = rate / 10;
    double best = -1e9;
    for (int s = 0; s + win <= static_cast<int>(y.size()); s += hop)
    {
        double e = 0;
        for (int i = s; i < s + win; ++i) e += y[static_cast<size_t>(i)] * y[static_cast<size_t>(i)];
        best = std::max(best, -0.691 + 10.0 * std::log10(e / win + 1e-20));
    }
    return best;
}

// Each mood's loudness against CALL for one voice, energy-averaged over the three notes.
std::vector<double> moodsAgainstCall(int voice)
{
    std::vector<double> level;
    for (int mode = 0; mode < 5; ++mode)
    {
        double energy = 0;
        for (int note : { 48, 55, 62 }) energy += std::pow(10.0, loudest400(render(voice, mode, note)) / 10.0);
        level.push_back(10.0 * std::log10(energy / 3.0));
    }
    std::vector<double> against;
    for (int mode = 0; mode < 5; ++mode) against.push_back(level[static_cast<size_t>(mode)] - level[0]);
    return against;
}
}

int main(int argc, char** argv)
{
    if (argc < 3 || (std::string(argv[1]) != "--check" && std::string(argv[1]) != "--write"))
    {
        std::cerr << "usage: PHOQERLevels --check|--write <Voice/LevelTables.h>\n";
        return 2;
    }
    int off = 0;
    double table[3][5];
    for (int v = 0; v < 3; ++v)
    {
        const auto against = moodsAgainstCall(v);
        std::printf("%-7s", voiceNames[v]);
        for (int m = 0; m < 5; ++m)
        {
            const auto residual = against[static_cast<size_t>(m)];
            table[v][m] = levels::modeTrimDb[v][m] - (m == 0 ? 0.0 : residual);     // CALL stays the reference
            std::printf("  %s %+5.2f dB", modeNames[m], residual);
            if (m > 0 && std::abs(residual) > tolerance) ++off;
        }
        std::printf("\n");
    }
    if (std::string(argv[1]) == "--check")
    {
        if (off > 0) { std::printf("LEVELS_OFF: %d moods more than %.1f dB from CALL. Run --write after checking by ear.\n", off, tolerance); return 1; }
        std::printf("LEVELS_OK: every mood within %.1f dB of CALL\n", tolerance);
        return 0;
    }
    // Rewrite the three built-in rows; the loaded voice's row and everything else in the file stay.
    std::ifstream in(argv[2]);
    std::stringstream whole;
    whole << in.rdbuf();
    auto text = whole.str();
    for (int v = 0; v < 3; ++v)
    {
        const auto marker = std::string("// ") + voiceNames[v];
        const auto at = text.find(marker);
        if (at == std::string::npos) { std::cerr << "row marker missing: " << marker << "\n"; return 2; }
        const auto lineStart = text.rfind('\n', at) + 1;
        char row[200];
        std::snprintf(row, sizeof row, "    { %.1ff, %5.1ff, %4.1ff, %4.1ff, %4.1ff },   ", table[v][0], table[v][1], table[v][2], table[v][3], table[v][4]);
        text.replace(lineStart, at - lineStart, row);
    }
    std::ofstream(argv[2]) << text;
    std::printf("LEVELS_WRITTEN: BURP, SQUEAL and GROAN rows in %s\n", argv[2]);
    return 0;
}
