#pragma once
// KYRAX keys (no JUCE here). Keys are made by the activation server: "KYRAX-XXXX-XXXX-XXXX-XXXX",
// gifts "KYRAX-GIFT-NAME-XXXX-...", 80 random bits. PHOQER only checks the shape; the server knows which
// keys exist (it keeps fingerprints only) and says what a key is.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace phoqer::keys
{
enum class Kind : uint8_t { bought = 1, gift = 2 };

// What the server said about a key.
struct Key
{
    uint8_t product = 1;          // 1 = KYRAX
    Kind kind = Kind::bought;
    int serial = 0;
    std::string name;             // gifts only
};

constexpr const char* alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";     // Crockford base32: no I, L, O, U

// The 16 random characters, read the way people paste them (case, spaces, O for 0, I or L for 1), or nothing.
inline std::optional<std::string> keyBody(const std::string& text)
{
    if (text.size() > 200) return std::nullopt;
    std::vector<std::string> groups(1);
    for (char c : text)
    {
        if (c == ' ' || c == 9 || c == 10 || c == 13) continue;     // tab, newline, carriage return
        if (c == '-') { if (! groups.back().empty()) groups.emplace_back(); continue; }
        groups.back() += (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
    if (groups.back().empty()) groups.pop_back();
    if (groups.size() < 5 || groups.front() != "KYRAX") return std::nullopt;
    std::string body;
    for (size_t g = groups.size() - 4; g < groups.size(); ++g) body += groups[g];
    if (body.size() != 16) return std::nullopt;
    for (auto& c : body)
    {
        if (c == 'O') c = '0';
        if (c == 'I' || c == 'L') c = '1';
        bool known = false;
        for (int a = 0; a < 32; ++a) known = known || alphabet[a] == c;
        if (! known) return std::nullopt;
    }
    return body;
}
}
