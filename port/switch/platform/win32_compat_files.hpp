#pragma once

#include "win32_compat.hpp"

#include <string_view>
#include <cstdint>

namespace srhd_awa::platform::win32_compat {
ImportAddress ResolveFileImport(std::string_view dll, std::string_view symbol);
bool CloseFileHandle(std::uint32_t token);
}  // namespace srhd_awa::platform::win32_compat
