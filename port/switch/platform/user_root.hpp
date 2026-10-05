#pragma once

#include <string>

namespace srhd_awa::platform::user_root {

void SetRoot(const std::string& root);
std::string ConfigDirectory();
std::string ResolveConfigPath(const std::string& path);
bool EnsureLayout();

}  // namespace srhd_awa::platform::user_root
