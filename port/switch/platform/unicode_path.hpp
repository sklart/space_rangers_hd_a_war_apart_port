#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace srhd_awa::platform {
// Filesystem paths use UTF-8 on the portable backend. Game resource keys and
// serialized AnsiStrings retain their original code page.
inline std::string PathUtf8(std::u16string_view wide) {
    std::string result;
    for (std::size_t i = 0; i < wide.size(); ++i) {
        auto code = static_cast<std::uint32_t>(wide[i]);
        if (code >= 0xd800 && code <= 0xdbff) {
            if (++i == wide.size() || wide[i] < 0xdc00 || wide[i] > 0xdfff) return {};
            code = 0x10000 + ((code - 0xd800) << 10) + (wide[i] - 0xdc00);
        } else if (code >= 0xdc00 && code <= 0xdfff) return {};
        if (code == 0) return {};
        if (code < 0x80) result.push_back(static_cast<char>(code));
        else if (code < 0x800) {
            result.push_back(static_cast<char>(0xc0 | (code >> 6)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else if (code < 0x10000) {
            result.push_back(static_cast<char>(0xe0 | (code >> 12)));
            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (code >> 18)));
            result.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        }
    }
    return result;
}
}  // namespace srhd_awa::platform
