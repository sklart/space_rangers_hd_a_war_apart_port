#pragma once

#include <cstdint>

namespace srhd_awa::platform::win32_compat {
using ImportAddress = void (*)();

ImportAddress ResolveImport(const char* library, const char* symbol);
std::uint32_t ResolvedImportCount();
std::uint32_t UnmappedImportCount();
std::uint32_t OptionalDisabledImportCount();
std::uint32_t PhysicalDllLoadCount();
}  // namespace srhd_awa::platform::win32_compat
