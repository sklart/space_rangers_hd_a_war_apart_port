#include "win32_compat_registry.hpp"

#include "win32_handles.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace srhd_awa::platform::win32_compat {
namespace {
constexpr std::uint32_t kHkcu = 0x80000001u;
constexpr std::uint32_t kHklm = 0x80000002u;
constexpr std::uint32_t kMoreData = 234u;
struct Key { std::string path; };
struct Value { std::uint32_t type; std::vector<std::uint8_t> bytes; };
std::mutex g_mutex;
std::unordered_map<std::string, std::unordered_map<std::string, Value>> g_keys;

std::string Fold(std::string text) {
  std::replace(text.begin(), text.end(), '\\', '/');
  for (char& c : text)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

std::string Narrow(const char16_t* text) {
  if (!text) return {};
  std::string result;
  for (; *text; ++text) {
    if (*text < 0x80) result.push_back(static_cast<char>(*text));
    else if (*text < 0x800) {
      result.push_back(static_cast<char>(0xc0 | (*text >> 6)));
      result.push_back(static_cast<char>(0x80 | (*text & 0x3f)));
    } else {
      result.push_back(static_cast<char>(0xe0 | (*text >> 12)));
      result.push_back(static_cast<char>(0x80 | ((*text >> 6) & 0x3f)));
      result.push_back(static_cast<char>(0x80 | (*text & 0x3f)));
    }
  }
  return result;
}

std::string RootPath(std::uint32_t root) {
  if (root == kHkcu) return "hkcu";
  if (root == kHklm) return "hklm";
  const auto key = std::static_pointer_cast<Key>(Handles().Lookup(root, HandleType::RegistryKey));
  return key ? key->path : std::string{};
}

std::uint32_t OpenA(std::uint32_t root, std::uint8_t* subkey,
                    std::uint32_t, std::uint32_t, std::uint32_t* output) {
  if (!output) return kErrorInvalidParameter;
  *output = 0;
  const auto base = RootPath(root);
  if (base.empty()) return kErrorInvalidHandle;
  const auto path = Fold(base + "/" + (subkey ? reinterpret_cast<char*>(subkey) : ""));
  std::lock_guard lock(g_mutex);
  if (!g_keys.contains(path)) return kErrorFileNotFound;
  *output = Handles().Allocate(HandleType::RegistryKey, std::make_shared<Key>(path));
  return *output ? kErrorSuccess : kErrorInvalidHandle;
}

std::uint32_t CreateW(std::uint32_t root, char16_t* subkey,
                      std::uint32_t, char16_t*, std::uint32_t,
                      std::uint32_t, void*, std::uint32_t* output,
                      std::uint32_t* disposition) {
  if (!output || !subkey) return kErrorInvalidParameter;
  *output = 0;
  const auto base = RootPath(root);
  if (base.empty()) return kErrorInvalidHandle;
  if (base.starts_with("hklm")) return kErrorAccessDenied;
  const auto path = Fold(base + "/" + Narrow(subkey));
  std::lock_guard lock(g_mutex);
  const auto [_, inserted] = g_keys.try_emplace(path);
  *output = Handles().Allocate(HandleType::RegistryKey, std::make_shared<Key>(path));
  if (disposition) *disposition = inserted ? 1u : 2u;
  return *output ? kErrorSuccess : kErrorInvalidHandle;
}

std::uint32_t SetW(std::uint32_t token, char16_t* name, std::uint32_t,
                   std::uint32_t type, void* bytes, std::uint32_t count) {
  const auto key = std::static_pointer_cast<Key>(
      Handles().Lookup(token, HandleType::RegistryKey));
  if (!key) return kErrorInvalidHandle;
  if (!bytes && count) return kErrorInvalidParameter;
  std::vector<std::uint8_t> data(count);
  if (count) std::memcpy(data.data(), bytes, count);
  std::lock_guard lock(g_mutex);
  g_keys[key->path][Fold(Narrow(name))] = Value{type, std::move(data)};
  return kErrorSuccess;
}

std::uint32_t QueryA(std::uint32_t token, std::uint8_t* name, void*,
                     std::uint32_t* type, std::uint8_t* bytes,
                     std::uint32_t* capacity) {
  const auto key = std::static_pointer_cast<Key>(
      Handles().Lookup(token, HandleType::RegistryKey));
  if (!key) return kErrorInvalidHandle;
  if (!capacity) return kErrorInvalidParameter;
  std::lock_guard lock(g_mutex);
  const auto found_key = g_keys.find(key->path);
  if (found_key == g_keys.end()) return kErrorFileNotFound;
  const auto found = found_key->second.find(Fold(name ? reinterpret_cast<char*>(name) : ""));
  if (found == found_key->second.end()) return kErrorFileNotFound;
  if (type) *type = found->second.type;
  const auto required = static_cast<std::uint32_t>(found->second.bytes.size());
  if (bytes && *capacity < required) { *capacity = required; return kMoreData; }
  if (bytes && required) std::memcpy(bytes, found->second.bytes.data(), required);
  *capacity = required;
  return kErrorSuccess;
}

std::uint32_t Close(std::uint32_t token) {
  if (token == kHkcu || token == kHklm) return kErrorSuccess;
  return Handles().Close(token, HandleType::RegistryKey)
      ? kErrorSuccess : kErrorInvalidHandle;
}

std::uint32_t Flush(std::uint32_t token) {
  if (token == kHkcu || token == kHklm) return kErrorSuccess;
  return Handles().IsValid(token, HandleType::RegistryKey)
      ? kErrorSuccess : kErrorInvalidHandle;
}

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

ImportAddress ResolveRegistryImport(std::string_view dll, std::string_view symbol) {
  if (dll != "advapi32.dll") return nullptr;
  if (symbol == "RegOpenKeyExA") return Address(&OpenA);
  if (symbol == "RegCreateKeyExW") return Address(&CreateW);
  if (symbol == "RegSetValueExW") return Address(&SetW);
  if (symbol == "RegQueryValueExA") return Address(&QueryA);
  if (symbol == "RegCloseKey") return Address(&Close);
  if (symbol == "RegFlushKey") return Address(&Flush);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat
