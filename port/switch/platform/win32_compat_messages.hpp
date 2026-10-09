#pragma once
#include "win32_compat.hpp"
#include "input_platform.hpp"
#include <cstdint>
#include <string_view>
namespace srhd_awa::platform::win32_compat {
ImportAddress ResolveMessageImport(std::string_view dll, std::string_view symbol);
void InjectHostInput(input_platform::RawInput input);
void WarpInputCursor(std::int32_t x, std::int32_t y);
void PostPaint(std::uint32_t window);
}
