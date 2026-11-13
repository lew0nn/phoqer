// Sound regression check: renders every built-in voice in every mood on fixed notes and compares a
// fingerprint of each render (overall level and eight tone bands, every 2048 samples) with the stored
// one. Any difference over the tolerance fails, so a change to the sound is never shipped by accident.
//
//   PHOQERSoundPrint --check <prints file>    exit 0 when the sound is unchanged, 1 with a report when not
//   PHOQERSoundPrint --write <prints file>    store the current sound (after a change has been approved by ear)
#include "Core/PhoqerEngine.h"
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
constexpr int rate = 44100, block = 128, frame = 2048;
constexpr double tolerance = 0.01;          // dB
constexpr double floorDb = -90.0;           // quieter frames are not compared
const char* voiceNames[] { "burp", "squeal", "groan" };
const char* modeNames[] { "call", "honk", "bark", "wail", "murmur" };
constexpr double bandsHz[] { 100, 200, 400, 800, 1600, 3200, 6400, 12800 };

// One render: C3, C4, C5 short, then C6 held.
std::vector<float> render(int voice, int mode)
{
    struct Note { double on, off; int note; };
    static constexpr Note phrase[] { { 0.05, 0.6, 48 }, { 0.75, 1.3, 60 }, { 1.45, 2.0, 72 }, { 2.15, 3.6, 84 } };
    auto engine = std::make_unique<PhoqerEngine>();
    engine->prepare(rate, block, 2);
    MacroState m;
    m.character = static_cast<SealCharacter>(voice);
    m.behaviorMode = static_cast<BehaviourMode>(mode);
    AudioBuffer buffer(2, block);
    std::vector<float> left;
    const int blocks = static_cast<int>(std::ceil(rate * 3.9 / block));
    for (int b = 0; b < blocks; ++b)
    {
        std::vector<MidiEvent> events;
        auto add = [&](double t, MidiEventType type, int note, float v)
        {
            const int s = static_cast<int>(std::llround(t * rate));
            if (s >= b * block && s < (b + 1) * block) events.push_back({ type, s - b * block, 0, note, v });
        };
        for (const auto& n : phrase) { add(n.on, MidiEventType::noteOn, n.note, 0.85f); add(n.off, MidiEventType::noteOff, n.note, 0.0f); }
        std::stable_sort(events.begin(), events.end(), [](const auto& x, const auto& y) { return x.sampleOffset < y.sampleOffset; });
        engine->process(buffer, events.data(), static_cast<int>(events.size()), m, 0.0f);
        for (int i = 0; i < block; ++i) left.push_back(buffer.getReadPointer(0)[i]);
    }
    return left;
}

// Per frame: overall level, then each band's level by Goertzel at its centre (Hann-windowed), in dB.
std::vector<double> fingerprint(const std::vector<float>& x)
{
    std::vector<double> out;
    for (size_t start = 0; start + frame <= x.size(); start += frame)
    {
        double sum = 0.0;
        for (int i = 0; i < frame; ++i) sum += static_cast<double>(x[start + i]) * x[start + i];
        out.push_back(10.0 * std::log10(std::max(1e-30, sum / frame)));
        for (const auto hz : bandsHz)
        {
            const double w = 2.0 * 3.14159265358979 * hz / rate, k = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (int i = 0; i < frame; ++i)
            {
                const double hann = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979 * i / (frame - 1));
                const double s0 = x[start + i] * hann + k * s1 - s2;
                s2 = s1; s1 = s0;
            }
            const double power = s1 * s1 + s2 * s2 - k * s1 * s2;
            out.push_back(10.0 * std::log10(std::max(1e-30, power / frame)));
        }
    }
    return out;
}
}

int main(int argc, char** argv)
{
    if (argc < 3 || (std::string(argv[1]) != "--check" && std::string(argv[1]) != "--write"))
    {
        std::cerr << "usage: PHOQERSoundPrint --check|--write <prints file>\n";
        return 2;
    }
    const bool write = std::string(argv[1]) == "--write";
    std::vector<std::pair<std::string, std::vector<double>>> now;
    for (int v = 0; v < 3; ++v)
        for (int mode = 0; mode < 5; ++mode)
            now.emplace_back(std::string(voiceNames[v]) + "-" + modeNames[mode], fingerprint(render(v, mode)));

    if (write)
    {
        std::ofstream file(argv[2]);
        file << "# PHOQER sound fingerprints: per render, per 2048-sample frame, level then 8 band levels (dB).\n"
             << "# Written by PHOQERSoundPrint --write after a sound change was approved by ear.\n";
        file.setf(std::ios::fixed);
        file.precision(4);
        for (const auto& [name, values] : now)
        {
            file << name;
            for (const auto v : values) file << ' ' << v;
            file << '\n';
        }
        std::cout << "SOUNDPRINT_WRITTEN: " << now.size() << " renders\n";
        return 0;
    }

    std::ifstream file(argv[2]);
    if (! file) { std::cerr << "no prints file: " << argv[2] << "\n"; return 2; }
    std::string line;
    int changed = 0, checked = 0;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        std::string name;
        in >> name;
        std::vector<double> stored;
        for (double v; in >> v;) stored.push_back(v);
        const auto found = std::find_if(now.begin(), now.end(), [&](const auto& r) { return r.first == name; });
        if (found == now.end() || found->second.size() != stored.size())
        {
            std::cout << name << ": render length or name changed\n";
            ++changed;
            continue;
        }
        double worst = 0.0;
        size_t at = 0;
        for (size_t i = 0; i < stored.size(); ++i)
        {
            if (stored[i] < floorDb && found->second[i] < floorDb) continue;
            const auto d = std::abs(stored[i] - found->second[i]);
            if (d > worst) { worst = d; at = i; }
        }
        if (worst > tolerance)
        {
            const auto seconds = static_cast<double>(at / 9) * frame / rate;
            std::cout << name << ": changed by " << worst << " dB at " << seconds << " s ("
                      << (at % 9 == 0 ? std::string("level") : std::to_string(static_cast<int>(bandsHz[at % 9 - 1])) + " Hz band") << ")\n";
            ++changed;
        }
        else ++checked;
    }
    if (checked + changed != static_cast<int>(now.size()))
    {
        std::cout << "prints file has " << checked + changed << " renders, expected " << now.size() << "\n";
        return 1;
    }
    if (changed > 0)
    {
        std::cout << "SOUNDPRINT_CHANGED: " << changed << " of " << now.size()
                  << " renders. If the change was approved by ear, run --write.\n";
        return 1;
    }
    std::cout << "SOUNDPRINT_OK: " << checked << " renders unchanged (tolerance " << tolerance << " dB)\n";
    return 0;
}
