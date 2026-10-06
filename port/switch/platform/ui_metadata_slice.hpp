#pragma once

#include <string>
#include <vector>

namespace srhd_awa::platform::ui_metadata_slice {

struct FontResolution {
  std::string key;
  bool found{};
  bool file{};
  std::string filename;
};

// Owns only release font-name metadata and a diagnostic snapshot.
// CacheDataRoot and GlobalCache stay owned by runtime_settings_slice.
bool Initialize(std::string* error);
void Shutdown();
const std::vector<FontResolution>& FontResolutions();

}  // namespace srhd_awa::platform::ui_metadata_slice