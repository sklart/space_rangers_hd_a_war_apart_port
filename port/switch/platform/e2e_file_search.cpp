#include "e2e_file_search.hpp"

#include "types/SysUtilsImports.hpp"
#include "types/WindowsImports.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace srhd_awa::platform::e2e_file_search {
namespace {
constexpr std::int32_t kNotFound = 2;
constexpr std::int32_t kNoMoreFiles = 18;
constexpr std::int32_t kIoError = 5;

struct Entry {
  std::string name;
  std::uint64_t size = 0;
  std::int32_t attributes = 0;
};
struct Search {
  std::vector<Entry> entries;
  std::size_t next = 0;
};

std::mutex g_mutex;
std::unordered_map<std::uint32_t, Search> g_searches;
std::uint32_t g_next_handle = 1;

void Fill(SysUtils::TSearchRec& record, const Entry& entry) {
  record.Name = pas::AnsiString(entry.name.c_str());
  record.Size = static_cast<std::int64_t>(entry.size);
  record.Attr = entry.attributes;
  record.Time = 0;  // The reached language search consumes Name only.
  record.FindData = {};
  record.FindData.dwFileAttributes = entry.attributes;
  record.FindData.nFileSizeHigh = static_cast<std::uint32_t>(entry.size >> 32);
  record.FindData.nFileSizeLow = static_cast<std::uint32_t>(entry.size);
  const auto length = std::min(entry.name.size(), sizeof(record.FindData.cFileName.elements) - 1);
  std::memcpy(record.FindData.cFileName.elements, entry.name.data(), length);
}
}  // namespace

std::int32_t First(const char* pattern, std::int32_t attributes, SysUtils::TSearchRec& record) {
  record.FindHandle = WindowsImports::INVALID_HANDLE_VALUE;
  if (!pattern || !*pattern) return kNotFound;
  std::string normalized(pattern);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  const std::filesystem::path input(normalized);
  const std::filesystem::path directory = input.has_parent_path() ? input.parent_path() : std::filesystem::path(".");
  const std::string mask = input.filename().string();
  std::error_code error;
  Search search;
  for (const auto& item : std::filesystem::directory_iterator(directory, error)) {
    if (error) break;
    const std::string name = item.path().filename().string();
    if (!MatchPattern(mask.c_str(), name.c_str())) continue;
    const bool is_directory = item.is_directory(error);
    if (error) break;
    const std::int32_t item_attributes = (is_directory ? SysUtilsImports::faDirectory : 0) |
        (!name.empty() && name.front() == '.' ? SysUtilsImports::faHidden : 0);
    if ((item_attributes & ~(attributes) &
         (SysUtilsImports::faDirectory | SysUtilsImports::faHidden | SysUtilsImports::faSysFile)) != 0) continue;
    const auto size = is_directory ? 0 : item.file_size(error);
    if (error) break;
    search.entries.push_back({name, size, item_attributes});
  }
  if (error) return kIoError;
  if (search.entries.empty()) return kNotFound;
  std::lock_guard lock(g_mutex);
  while (g_next_handle == 0 || g_next_handle == WindowsImports::INVALID_HANDLE_VALUE ||
         g_searches.contains(g_next_handle)) ++g_next_handle;
  record.FindHandle = g_next_handle++;
  Fill(record, search.entries.front());
  search.next = 1;
  g_searches.emplace(record.FindHandle, std::move(search));
  return 0;
}

std::int32_t Next(SysUtils::TSearchRec& record) {
  std::lock_guard lock(g_mutex);
  const auto it = g_searches.find(record.FindHandle);
  if (it == g_searches.end() || it->second.next >= it->second.entries.size()) return kNoMoreFiles;
  Fill(record, it->second.entries[it->second.next++]);
  return 0;
}

void Close(SysUtils::TSearchRec& record) {
  std::lock_guard lock(g_mutex);
  g_searches.erase(record.FindHandle);
  record.FindHandle = WindowsImports::INVALID_HANDLE_VALUE;
}

}  // namespace srhd_awa::platform::e2e_file_search
