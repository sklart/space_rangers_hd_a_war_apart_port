#include "game_path.hpp"
#include "units/SysUtilsImports.hpp"

namespace SysUtilsImports {
std::uint8_t FileExists(const pas::AnsiString& file_name) {
  return srhd_awa::platform::game_path::FileExists(file_name.c_str());
}

void PAS_STDCALL Sleep(std::uint32_t) {}
}  // namespace SysUtilsImports
