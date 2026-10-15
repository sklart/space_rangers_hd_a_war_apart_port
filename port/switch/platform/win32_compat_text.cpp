#include "win32_compat_text.hpp"

#include "e2e_stage.hpp"
#include "win32_handles.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#if defined(__SWITCH__)
#include <SDL2/SDL.h>
#endif

namespace srhd_awa::platform::win32_compat {
namespace {
std::uint8_t LowerAnsi(std::uint8_t value) {
  if (value >= 'A' && value <= 'Z') return static_cast<std::uint8_t>(value + 32);
  if (value >= 0xc0 && value <= 0xdf) return static_cast<std::uint8_t>(value + 32);
  if (value == 0xa8) return 0xb8; // CP1251 Ё.
  return value;
}
char16_t LowerWide(char16_t value) {
  if (value >= u'A' && value <= u'Z') return value + 32;
  if (value >= 0x0410 && value <= 0x042f) return value + 32;
  if (value == 0x0401) return 0x0451;
  return value;
}
char16_t UpperWide(char16_t value) {
  if (value >= u'a' && value <= u'z') return value - 32;
  if (value >= 0x0430 && value <= 0x044f) return value - 32;
  if (value == 0x0451) return 0x0401;
  return value;
}

std::uint8_t* CharNext(std::uint8_t* value) {
  return value && *value ? value + 1 : value;
}
std::uint32_t CharLowerBuffA(std::uint8_t* text, std::uint32_t length) {
  if (!text && length) { SetLastError(kErrorInvalidParameter); return 0; }
  for (std::uint32_t i = 0; i < length; ++i) text[i] = LowerAnsi(text[i]);
  SetLastError(kErrorSuccess);
  return length;
}
std::uint32_t CharLowerBuffW(char16_t* text, std::uint32_t length) {
  if (!text && length) { SetLastError(kErrorInvalidParameter); return 0; }
  for (std::uint32_t i = 0; i < length; ++i) text[i] = LowerWide(text[i]);
  SetLastError(kErrorSuccess);
  return length;
}
std::uint32_t CharUpperBuffW(char16_t* text, std::uint32_t length) {
  if (!text && length) { SetLastError(kErrorInvalidParameter); return 0; }
  for (std::uint32_t i = 0; i < length; ++i) text[i] = UpperWide(text[i]);
  SetLastError(kErrorSuccess);
  return length;
}

std::int32_t CompareStringA(std::uint32_t, std::uint32_t flags,
                             std::uint8_t* first, std::int32_t first_length,
                             std::uint8_t* second, std::int32_t second_length) {
  if (!first || !second || first_length < -1 || second_length < -1) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  const auto a = first_length < 0 ? std::strlen(reinterpret_cast<char*>(first)) + 1
                                  : static_cast<std::size_t>(first_length);
  const auto b = second_length < 0 ? std::strlen(reinterpret_cast<char*>(second)) + 1
                                   : static_cast<std::size_t>(second_length);
  const auto common = std::min(a, b);
  for (std::size_t i = 0; i < common; ++i) {
    const auto left = flags & 1 ? LowerAnsi(first[i]) : first[i];
    const auto right = flags & 1 ? LowerAnsi(second[i]) : second[i];
    if (left != right) return left < right ? 1 : 3;
  }
  return a < b ? 1 : a > b ? 3 : 2;
}

std::uint32_t FormatMessageA(std::uint32_t flags, void*, std::uint32_t error,
                              std::uint32_t, std::uint8_t* output,
                              std::uint32_t capacity, void*) {
  if (!output || !capacity || (flags & 0x100u)) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  std::string message;
  switch (error) {
    case kErrorFileNotFound: message = "The system cannot find the file specified."; break;
    case kErrorPathNotFound: message = "The system cannot find the path specified."; break;
    case kErrorAccessDenied: message = "Access is denied."; break;
    case kErrorInvalidHandle: message = "The handle is invalid."; break;
    case kErrorInvalidParameter: message = "The parameter is incorrect."; break;
    default: SetLastError(kErrorFileNotFound); return 0;
  }
  if (message.size() + 1 > capacity) { SetLastError(kErrorInvalidParameter); return 0; }
  std::memcpy(output, message.c_str(), message.size() + 1);
  SetLastError(kErrorSuccess);
  return static_cast<std::uint32_t>(message.size());
}

std::int32_t MessageBoxA(std::uint32_t, std::uint8_t* text,
                          std::uint8_t* caption, std::uint32_t) {
  const auto* body = text ? reinterpret_cast<const char*>(text) : "";
  const auto* title = caption ? reinterpret_cast<const char*>(caption) : "Space Rangers";
#if defined(__SWITCH__)
  if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title, body, nullptr) != 0) {
    SetLastError(kErrorAccessDenied);
    return 0;
  }
#else
  (void)title;
#endif
  e2e_stage::LogWinApi(body);
  SetLastError(kErrorSuccess);
  return 1;
}

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

ImportAddress ResolveTextImport(std::string_view dll, std::string_view symbol) {
  if (dll == "user32.dll") {
    if (symbol == "CharNextA") return Address(&CharNext);
    if (symbol == "CharLowerBuffA") return Address(&CharLowerBuffA);
    if (symbol == "CharLowerBuffW") return Address(&CharLowerBuffW);
    if (symbol == "CharUpperBuffW") return Address(&CharUpperBuffW);
    if (symbol == "MessageBoxA") return Address(&MessageBoxA);
  }
  if (dll == "kernel32.dll") {
    if (symbol == "CompareStringA") return Address(&CompareStringA);
    if (symbol == "FormatMessageA") return Address(&FormatMessageA);
  }
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat
