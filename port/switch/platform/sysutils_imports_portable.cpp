#include "game_path.hpp"
#include "user_root.hpp"
#include "units/SysUtilsImports.hpp"

#include <filesystem>

namespace SysUtilsImports {
std::uint8_t FileExists(const pas::AnsiString& file_name) {
  const std::string user_path = srhd_awa::platform::user_root::ResolveConfigPath(file_name.c_str());
  std::error_code error;
  if (!user_path.empty() && std::filesystem::is_regular_file(user_path, error) && !error) return true;
  return srhd_awa::platform::game_path::FileExists(file_name.c_str());
}

}  // namespace SysUtilsImports
