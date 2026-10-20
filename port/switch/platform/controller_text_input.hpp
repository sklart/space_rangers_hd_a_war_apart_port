#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace srhd_awa::platform::controller_text_input {
struct Request {
  std::u16string initial;
  std::size_t min_length{}, max_length{}; // UTF-16 units, matching TEditGI.
  const char* header{"Введите текст"};
};
using Backend = bool (*)(const Request&, std::u16string&, void*);
inline bool Valid(std::u16string_view text, std::size_t minimum, std::size_t maximum) {
  if (minimum > maximum || maximum > 4096 || text.size() < minimum || text.size() > maximum) return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto ch = text[i];
    if (ch < 0x20 || ch == 0x7f) return false;
    if (ch >= 0xd800 && ch <= 0xdbff) {
      if (++i == text.size() || text[i] < 0xdc00 || text[i] > 0xdfff) return false;
    } else if (ch >= 0xdc00 && ch <= 0xdfff) return false;
  }
  return true;
}
// Cancel, malformed input and exceptions preserve the caller's complete value.
inline bool Prompt(std::u16string& value, std::size_t minimum, std::size_t maximum,
                   const char* header, Backend backend, void* user = nullptr) {
  if (!backend || minimum > maximum || maximum > 4096 || value.size() > 4096) return false;
  Request request{value, minimum, maximum, header};
  std::u16string candidate;
  if (!backend(request, candidate, user) || !Valid(candidate, minimum, maximum)) return false;
  value = std::move(candidate);
  return true;
}
} // namespace srhd_awa::platform::controller_text_input
