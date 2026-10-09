#pragma once
#include "win32_compat.hpp"
#include "types/Windows_group.hpp"
#include <cstdint>
#include <string_view>
namespace srhd_awa::platform::win32_compat {
Windows::TFileTime UnixMillisToFileTime(std::int64_t milliseconds);
ImportAddress ResolveFileTimeImport(std::string_view dll, std::string_view symbol);
}
