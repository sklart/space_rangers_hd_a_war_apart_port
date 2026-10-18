// Same valid fixtures run against unmodified MSVC x86 CBuf and the native port.
#include "CBuf.hpp"
#include "utf16.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <vector>
#ifdef NDEBUG
#error Buffer contract requires active assertions
#endif
using Base::CBuf;
using matrixgame::native::EncodeUtf16LE;
using matrixgame::native::DecodeUtf16LE;

std::vector<uint8_t> contents(const CBuf& b) {
    if (!b.Len()) return {};
    const auto* p = static_cast<const uint8_t*>(b.Get());
    return {p, p + b.Len()};
}
template<class Fn> void rejects(Fn action) {
    bool rejected = false;
    try { action(); } catch (const Base::CException& e) {
        rejected = true;
        assert(e.Info().find(L"Line=") != std::wstring::npos);
    }
    assert(rejected);
}
int main() {
    const std::wstring text = L"A\u0416\u6c34\U0001f680";
    const std::vector<uint8_t> expected{65,0,0x16,4,0x34,0x6c,0x3d,0xd8,0x80,0xde,0,0};
    CBuf b;
    b.WStr(text);
    assert(contents(b) == expected);
    b.Pointer(0); assert(b.WStr() == text); assert(b.Pointer() == expected.size());
    b.Clear(); b.WStr(L""); assert(contents(b) == std::vector<uint8_t>({0,0}));
    b.Pointer(0); assert(b.WStr().empty() && b.Pointer() == 2);
    b.Clear(); b.WStrNZ(text);
    assert(contents(b) == std::vector<uint8_t>(expected.begin(), expected.end() - 2));
    b.Clear(); b.WStrText(text);
    auto line = std::vector<uint8_t>(expected.begin(), expected.end() - 2);
    line.insert(line.end(), {13,0,10,0});
    assert(contents(b) == line);
    b.Pointer(0); assert(b.WStrText() == text && b.Pointer() == line.size());
    b.Clear(); b.WStrNZ(text); b.Pointer(0);
    assert(b.WStrText() == text && b.Pointer() == b.Len());
    for (auto terminators : {std::array<uint8_t,4>{0,0,0,0}, {13,0,10,0}, {10,0,13,0}, {10,0,10,0}}) {
        b.Clear(); b.WStrNZ(L"X"); b.Add(terminators.data(), terminators.size()); b.WStr(L"Y");
        b.Pointer(0); assert(b.WStrText() == L"X"); assert(b.Pointer() == 6); assert(b.WStr() == L"Y");
    }
    // Mixed 16-bit values, including paired/unpaired surrogates, at odd addresses.
    std::uint32_t rng = 0x438df12bu;
    std::uint32_t digest = 2166136261u;
    unsigned fixtures = 0;
    for (unsigned length = 0; length <= 512; ++length) {
        std::vector<uint8_t> raw;
        for (unsigned i = 0; i < length; ++i) {
            rng = rng * 1664525u + 1013904223u;
            uint16_t unit = static_cast<uint16_t>(rng >> 8);
            if (!unit) unit = 1;
            raw.push_back(static_cast<uint8_t>(unit)); raw.push_back(static_cast<uint8_t>(unit >> 8));
        }
        // Force a valid pair and lone surrogates independent of RNG coverage.
        raw.insert(raw.end(), {0,0xd8,0,0xdc,1,0xd8,65,0,1,0xdc,0,0});
        b.Clear(); b.ByteLoop(0x59, 3); b.Add(raw.data(), raw.size()); b.Pointer(3);
        const auto decoded = b.WStr();
        assert(b.Pointer() == 3 + raw.size());
        CBuf encoded; encoded.WStr(decoded); assert(contents(encoded) == raw);
        for (uint8_t ch : raw) digest = (digest ^ ch) * 16777619u;
        ++fixtures;
    }
    // Exhaust every nonzero code unit together; zero is the final terminator.
    std::vector<uint8_t> all_units;
    for (uint32_t unit = 1; unit <= 0xffff; ++unit) {
        all_units.push_back(static_cast<uint8_t>(unit));
        all_units.push_back(static_cast<uint8_t>(unit >> 8));
    }
    all_units.insert(all_units.end(), {0,0});
    b.Clear(); b.Add(all_units.data(), all_units.size()); b.Pointer(0);
    const auto all_text = b.WStr();
    CBuf all_encoded; all_encoded.WStr(all_text);
    assert(contents(all_encoded) == all_units && b.Pointer() == all_units.size());
    for (uint8_t ch : all_units) digest = (digest ^ ch) * 16777619u;
    ++fixtures;
    b.Clear(); b.ByteLoop(0x59, 1); b.Add<uint32_t>(0x89abcdefu); b.WordLoop(0xa135, 2);
    b.Pointer(1); assert(b.Get<uint32_t>() == 0x89abcdefu);
    assert(b.Get<uint16_t>() == 0xa135 && b.Get<uint16_t>() == 0xa135);
#ifndef MATRIXGAME_ORIGINAL_BUFFER_REFERENCE
    // Invalid input must fail before cursor movement or destination mutation.
    for (const auto& bytes : std::vector<std::vector<uint8_t>>{{}, {65}, {65,0}, {65,0,66}}) {
        b.Clear(); b.Add(bytes.data(), bytes.size()); b.Pointer(0);
        rejects([&]{ (void)b.WStr(); }); assert(b.Pointer() == 0);
    }
    b.Clear(); b.Add<uint8_t>(65); b.Pointer(0);
    rejects([&]{ (void)b.WStrText(); }); assert(b.Pointer() == 0);
    b.Clear(); b.Add<uint8_t>(3); b.Pointer(0);
    rejects([&]{ (void)b.Get<uint64_t>(); }); assert(b.Pointer() == 0);
    uint8_t scratch = 0;
    volatile size_t oversized = std::numeric_limits<size_t>::max();
    rejects([&]{ b.Get(&scratch, oversized); });
    assert(scratch == 0);
    const auto before = contents(b);
    rejects([&]{ b.ByteLoop(0, oversized); }); assert(contents(b) == before);
    rejects([&]{ b.WordLoop(0, oversized); }); assert(contents(b) == before);
    rejects([&]{ b.Expand(oversized); }); assert(contents(b) == before);
    rejects([&]{ b.Pointer(2); }); assert(b.Pointer() == 0);
    bool odd = false;
    try { (void)DecodeUtf16LE(std::array<uint8_t,1>{0}); } catch (const std::invalid_argument&) { odd = true; }
    assert(odd);
    if constexpr (sizeof(wchar_t) == 4) {
        for (uint32_t invalid : {0x110000u, 0xffffffffu}) {
            bool bad = false;
            try { b.WStr(std::wstring(1, static_cast<wchar_t>(invalid))); }
            catch (const std::invalid_argument&) { bad = true; }
            assert(bad && contents(b) == before);
        }
    }
    Base::CExceptionStr message("test.cpp", 42, L"first");
    assert(message.Info().find(L"Line=42") != std::wstring::npos);
    assert(message.Info().find(L"Text: first") != std::wstring::npos);
#endif
    assert(digest == 0x89542cb4u); // Original MSVC x86 byte transcript.
    std::printf("{\"buffer_fixtures\":%u,\"transcript_fnv32\":\"%08x\"}\n", fixtures, digest);
}
