#pragma once
#include "utf16.hpp"
#include "CException.hpp"
#include <codecvt>
#include <locale>
#include <string>

namespace matrixgame::native {
inline std::u16string Utf16Units(std::wstring_view text) {
    const auto bytes = EncodeUtf16LE(text);
    std::u16string result;
    result.reserve(bytes.size() / 2);
    for (size_t i = 0; i < bytes.size(); i += 2) result.push_back(char16_t(Utf16Unit(bytes.data() + i)));
    return result;
}
// Use the same UTF-8/UTF-16 conversion boundary as original Windows utils,
// while keeping the platform's ordinary wchar_t ABI.
inline std::string ConfigUtf8(std::wstring_view text) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.to_bytes(Utf16Units(text));
}
inline std::wstring ConfigWide(std::string_view text) {
    if (text.empty()) return {};
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    const auto units = converter.from_bytes(text.data(), text.data() + text.size());
    std::vector<uint8_t> bytes;
    bytes.reserve(units.size() * 2);
    for (auto unit : units) { bytes.push_back(uint8_t(unit)); bytes.push_back(uint8_t(unit >> 8)); }
    return DecodeUtf16LE(bytes);
}
class ConfigDepthGuard {
    unsigned& depth_;
public:
    explicit ConfigDepthGuard(unsigned& depth) : depth_(depth) {
        if (depth_ >= 128) ERROR_S(L"MatrixLib configuration recursion limit");
        ++depth_;
    }
    ~ConfigDepthGuard() { --depth_; }
    ConfigDepthGuard(const ConfigDepthGuard&) = delete;
    ConfigDepthGuard& operator=(const ConfigDepthGuard&) = delete;
};
}
