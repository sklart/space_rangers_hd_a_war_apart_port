#pragma once
#include "win32_compat.hpp"
#include <string_view>
namespace srhd_awa::platform::win32_compat {
ImportAddress ResolveWinmmImport(std::string_view dll, std::string_view symbol);
}
