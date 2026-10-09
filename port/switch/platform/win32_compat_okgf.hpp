#pragma once
#include "win32_compat.hpp"
#include <string_view>
namespace srhd_awa::platform::win32_compat {
ImportAddress ResolveOkgfImport(std::string_view symbol);
}
