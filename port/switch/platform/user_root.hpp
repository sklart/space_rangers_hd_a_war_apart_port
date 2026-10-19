#pragma once

#include <string>

namespace srhd_awa::platform::user_root {

void SetRoot(const std::string& root);
std::string ConfigDirectory();
std::string ResolveConfigPath(const std::string& path);
std::string ResolveConfigPath(const char16_t* path);
// Empty for resource keys and paths outside the selected user config root.
std::string ResolveAbsoluteConfigPath(const char16_t* path);
bool ConfigFileExists(const char16_t* path);
bool EnsureLayout();

}  // namespace srhd_awa::platform::user_root
