#pragma once

#include "win32_compat.hpp"

#include <string_view>

namespace srhd_awa::platform::win32_compat {
ImportAddress ResolveSyncImport(std::string_view dll, std::string_view symbol);
bool IsThreadUsable(std::uint32_t token);
bool PreferCalculationCore(std::uint32_t token);
}
