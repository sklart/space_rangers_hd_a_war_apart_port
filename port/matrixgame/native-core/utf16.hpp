#pragma once
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace matrixgame::native {
// On-disk MatrixLib strings are UTF-16LE regardless of host wchar_t width.
// Lone surrogates round-trip as code units; valid pairs become one wchar_t on
// UTF-32 hosts. No replacement, normalization, locale or short-wchar ABI switch.
inline std::uint16_t Utf16Unit(const std::uint8_t* p) noexcept {
    return std::uint16_t(p[0]) | (std::uint16_t(p[1]) << 8);
}
inline std::wstring DecodeUtf16LE(std::span<const std::uint8_t> bytes) {
    static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);
    if (bytes.size() & 1u) throw std::invalid_argument("odd UTF-16 byte count");
    std::wstring text;
    text.reserve(bytes.size() / 2);
    for (std::size_t offset = 0; offset < bytes.size(); offset += 2) {
        std::uint32_t ch = Utf16Unit(bytes.data() + offset);
        if constexpr (sizeof(wchar_t) == 4) {
            if (ch >= 0xd800 && ch <= 0xdbff && offset + 2 < bytes.size()) {
                const auto low = Utf16Unit(bytes.data() + offset + 2);
                if (low >= 0xdc00 && low <= 0xdfff) {
                    ch = 0x10000 + ((ch - 0xd800) << 10) + (low - 0xdc00);
                    offset += 2;
                }
            }
        }
        text.push_back(static_cast<wchar_t>(ch));
    }
    return text;
}
inline std::vector<std::uint8_t> EncodeUtf16LE(std::wstring_view text) {
    static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);
    std::vector<std::uint8_t> bytes;
    if (text.size() > bytes.max_size() / 4) throw std::length_error("UTF-16 length overflow");
    bytes.reserve(text.size() * 2);
    const auto unit = [&](std::uint32_t ch) {
        bytes.push_back(static_cast<std::uint8_t>(ch));
        bytes.push_back(static_cast<std::uint8_t>(ch >> 8));
    };
    for (wchar_t value : text) {
        const auto ch = static_cast<std::uint32_t>(value);
        if constexpr (sizeof(wchar_t) == 4) {
            if (ch > 0x10ffff) throw std::invalid_argument("wide character out of Unicode range");
            if (ch > 0xffff) {
                unit(0xd800 + ((ch - 0x10000) >> 10));
                unit(0xdc00 + ((ch - 0x10000) & 1023));
                continue;
            }
        }
        unit(ch);
    }
    return bytes;
}
} // namespace matrixgame::native
