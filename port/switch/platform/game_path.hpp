#pragma once

#include <string>

namespace srhd_awa::platform::game_path {
void SetRoot(const std::string& root);
std::string Resolve(const std::string& game_relative_path);
bool FileExists(const std::string& game_relative_path);
}  // namespace srhd_awa::platform::game_path
