// Native UTF-16 disk boundary for the original MatrixLib CBuf class.
#include "CBuf.hpp"
#include "utf16.hpp"
#include <limits>

namespace Base {
using matrixgame::native::DecodeUtf16LE;
using matrixgame::native::EncodeUtf16LE;
using matrixgame::native::Utf16Unit;

size_t CBuf::WStrLen() {
    if (m_Pointer > m_Buf.size()) ERROR_E;
    const auto remaining = m_Buf.size() - m_Pointer;
    for (size_t offset = 0; remaining - offset >= 2; offset += 2)
        if (Utf16Unit(m_Buf.data() + m_Pointer + offset) == 0) return offset / 2;
    ERROR_E; // Missing terminator or truncated last unit: do not read beyond EOF.
}
size_t CBuf::WStrTextLen() {
    if (m_Pointer > m_Buf.size()) ERROR_E;
    const auto remaining = m_Buf.size() - m_Pointer;
    for (size_t offset = 0; remaining - offset >= 2; offset += 2) {
        const auto ch = Utf16Unit(m_Buf.data() + m_Pointer + offset);
        if (ch == 0 || ch == 13 || ch == 10) return offset / 2;
    }
    if (remaining & 1u) ERROR_E;
    return remaining / 2;
}
std::wstring CBuf::WStr() {
    const auto count = WStrLen() * 2;
    auto result = DecodeUtf16LE(std::span<const uint8_t>(m_Buf).subspan(m_Pointer, count));
    m_Pointer += count + 2;
    return result;
}
void CBuf::WStr(const std::wstring& text) {
    auto bytes = EncodeUtf16LE(text); // Complete conversion before changing CBuf.
    if (bytes.size() > bytes.max_size() - 2) throw std::length_error("UTF-16 terminator overflow");
    bytes.push_back(0); bytes.push_back(0);
    Add(bytes.data(), bytes.size());
}
void CBuf::WStrNZ(const std::wstring& text) {
    const auto bytes = EncodeUtf16LE(text);
    Add(bytes.data(), bytes.size());
}
std::wstring CBuf::WStrText() {
    const auto count = WStrTextLen() * 2;
    auto result = DecodeUtf16LE(std::span<const uint8_t>(m_Buf).subspan(m_Pointer, count));
    auto next = m_Pointer + count;
    // Preserve original MatrixLib consumption of up to two terminator units.
    for (int i = 0; i < 2 && m_Buf.size() - next >= 2; ++i) {
        const auto ch = Utf16Unit(m_Buf.data() + next);
        if (ch != 0 && ch != 13 && ch != 10) break;
        next += 2;
    }
    m_Pointer = next;
    return result;
}
void CBuf::WStrText(const std::wstring& text) {
    auto bytes = EncodeUtf16LE(text);
    if (bytes.size() > bytes.max_size() - 4) throw std::length_error("UTF-16 CRLF overflow");
    bytes.insert(bytes.end(), {13, 0, 10, 0});
    Add(bytes.data(), bytes.size());
}
} // namespace Base
