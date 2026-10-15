#include "e2e_file_search.hpp"

#include <cstring>

namespace srhd_awa::platform::e2e_file_search {
namespace {
unsigned char Fold(unsigned char c) {
  return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
}
}
bool MatchPattern(const char* pattern, const char* name) {
  if (!pattern || !name) return false;
  // Win32 FindFirstFile treats *.* as all names, including extensionless names.
  if (std::strcmp(pattern, "*.*") == 0) return true;
  const char* star = nullptr;
  const char* retry = nullptr;
  while (*name) {
    if (*pattern == '?' || (*pattern && Fold(*pattern) == Fold(*name))) {
      ++pattern;
      ++name;
    } else if (*pattern == '*') {
      star = pattern++;
      retry = name;
    } else if (star) {
      pattern = star + 1;
      name = ++retry;
    } else {
      return false;
    }
  }
  while (*pattern == '*') ++pattern;
  return *pattern == 0;
}
}  // namespace srhd_awa::platform::e2e_file_search
