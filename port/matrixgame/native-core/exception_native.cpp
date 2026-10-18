// Native implementation of the original MatrixLib exception API.
// Reports file/line and message; the original Win32/x86 Tracer is not linked.
#include "CException.hpp"
#include <sstream>

namespace {
std::wstring DiagnosticFile(const char* file) {
    std::wstring result;
    if (!file) return result;
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(file); *p; ++p) {
        if (*p >= 32 && *p < 127) result.push_back(static_cast<wchar_t>(*p));
        else {
            result += L"\\x";
            result.push_back(hex[*p >> 4]); result.push_back(hex[*p & 15]);
        }
    }
    return result;
}
}
namespace Base {
CException::CException(const char* file, int line) : m_File(file), m_Line(line) {}
std::wstring CException::Info() const {
    std::wostringstream out;
    out << L"File=" << DiagnosticFile(m_File) << L"\nLine=" << m_Line << L"\n";
    return out.str();
}
CExceptionStr::CExceptionStr(const char* file, int line, const wchar* first, const wchar* second)
    : CException(file, line) {
    if (first) m_str += first;
    if (second) m_str += second;
}
std::wstring CExceptionStr::Info() const { return CException::Info() + L"Text: " + m_str.c_str(); }
} // namespace Base
