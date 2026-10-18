"""Port pinned original BlockPar parser and enable the real storage bridge."""
INPUTS = {'CBlockPar.cpp': 'bc2c55bddd62709bad774569f267c34b9e0c08b910d1be5a40924a62fa83ce0a'}


def patch_blockpar(files, body):
    utils = files['utils.hpp'].decode()
    utils = utils.replace('#include <codecvt>', '#include "config_unicode.hpp"')
    utils = body(utils, 'inline std::string from_wstring(std::wstring_view wstr)', '''inline std::string from_wstring(std::wstring_view wstr) {
    return matrixgame::native::ConfigUtf8(wstr);
}''')
    utils = body(utils, 'inline std::wstring to_wstring(std::string_view str)', '''inline std::wstring to_wstring(std::string_view str) {
    return matrixgame::native::ConfigWide(str);
}''')
    utils = body(utils, 'inline std::wstring format(const wchar_t* format, Args... args)', """inline std::wstring format(const wchar_t* format, Args... args) {
    // Keep the original intended 10240-character bound, off the Switch stack.
    size_t capacity = 128;
    for (;;) {
        std::vector<wchar_t> buffer(capacity);
        const auto length = std::swprintf(buffer.data(), buffer.size(), format, args...);
        if (length >= 0 && size_t(length) < buffer.size()) return std::wstring(buffer.data(), size_t(length));
        if (capacity == 10240) throw std::runtime_error("swprintf() failed");
        capacity = capacity > 5120 ? 10240 : capacity * 2;
    }
}""")
    files['utils.hpp'] = utils.encode()
    source = files['CBlockPar.cpp'].decode().replace('DTRACE();', '')
    source = source.replace('#include <utils.hpp>', '#include <utils.hpp>\n#include "storage_limits.hpp"\n#include <bit>\n#include <climits>')
    source = body(source, 'int ParamParser::GetInt() const', '''int ParamParser::GetInt() const {
    uint32_t value = 0;
    bool negative = false;
    for (auto unit : matrixgame::native::Utf16Units(*this)) {
        const auto ch = uint16_t(unit - u'0');
        if (ch < 10) value = value * 10 + ch;
        if (unit == u'-') negative = true;
    }
    return std::bit_cast<int32_t>(negative ? uint32_t(0) - value : value);
}''')
    source = body(source, 'uint32_t ParamParser::GetDword() const', '''uint32_t ParamParser::GetDword() const {
    uint32_t value = 0;
    for (auto unit : matrixgame::native::Utf16Units(*this)) {
        const auto ch = uint16_t(unit - u'0');
        if (ch < 10) value = value * 10 + ch;
    }
    return value;
}''')
    source = body(source, 'uint32_t ParamParser::GetHexUnsigned(void) const', '''uint32_t ParamParser::GetHexUnsigned(void) const {
    uint32_t value = 0;
    for (auto unit : matrixgame::native::Utf16Units(*this)) {
        auto ch = uint16_t(unit - u'0');
        if (ch > 9) ch = uint16_t((uint32_t(unit) & ~uint32_t(32)) - ('A' - 10));
        value = (value << 4) + ch;
    }
    return value;
}''')
    source = source.replace('''int CBlockPar::LoadFromText(const std::wstring &text) {''', '''int CBlockPar::LoadFromText(const std::wstring &text) {
    if (text.size() > matrixgame::native::StorageLimit / sizeof(wchar_t)) ERROR_E;
    static thread_local unsigned depth = 0;
    matrixgame::native::ConfigDepthGuard guard(depth);''')
    source = body(source, 'void CBlockPar::LoadFromTextFile(const std::wstring &filename)', '''void CBlockPar::LoadFromTextFile(const std::wstring &filename) {
    CFile file(filename); file.OpenRead();
    const auto size = file.SizeFull();
    if (size < 0 || uint64_t(size) > matrixgame::native::StorageLimit) ERROR_E;
    if (!size) return; // Original empty-file behavior leaves existing parameters intact.
    std::string bytes(size_t(size), '\\0');
    file.Read(bytes.data(), bytes.size());
    if (bytes.size() >= 2 && uint8_t(bytes[0]) == 0xff && uint8_t(bytes[1]) == 0xfe) {
        if (bytes.size() == 2) return; // Original BOM-only file leaves old parameters intact.
        const auto* first = reinterpret_cast<const uint8_t*>(bytes.data()) + 2;
        LoadFromText(matrixgame::native::DecodeUtf16LE(std::span<const uint8_t>(first, bytes.size() - 2)));
    } else {
        LoadFromText(utils::to_wstring(bytes.c_str()));
    }
}''')
    source = source.replace('''void CBlockPar::SaveInText(CBuf &buf, bool ansi, int level) {''', '''void CBlockPar::SaveInText(CBuf &buf, bool ansi, int level) {
    if (level < 0 || level >= 128) ERROR_E;''')
    files['CBlockPar.cpp'] = source.encode()
    storage = files['CStorage_blockpar.cpp'].decode().replace('DTRACE();', '')
    for signature in ['void CStorage::StoreBlockPar(const wchar *root, const CBlockPar &bp)',
                      'void CStorage::RestoreBlockPar(const wchar *root, CBlockPar &bp)']:
        storage = storage.replace(signature + ' {', signature + ''' {
    if (!root) ERROR_E;
    static thread_local unsigned depth = 0;
    matrixgame::native::ConfigDepthGuard guard(depth);''')
    storage = storage.replace('''    for (dword i = 0; i < propkey->GetArraysCount(); ++i) {''', '''    if (!propkey || !propval || propkey->GetArraysCount() != propval->GetArraysCount()) ERROR_E;
    for (dword i = 0; i < propkey->GetArraysCount(); ++i) {''')
    files['CStorage_blockpar.cpp'] = storage.encode()
