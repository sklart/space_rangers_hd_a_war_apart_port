#pragma once
#include "win32_compat.hpp"
#include "types/Windows_group.hpp"
#include <cstdint>
#include <string_view>
namespace srhd_awa::platform::win32_compat {
std::uint32_t RegisterMainWindow(void* native_window, std::int32_t width,
                                 std::int32_t height);
void UnregisterMainWindow(std::uint32_t token);
std::uint32_t MainWindow();
void MainWindowSize(std::int32_t* width, std::int32_t* height);
std::int32_t DispatchWindowMessage(const Windows::TMsg& message);
void SetInputCursor(std::int32_t x, std::int32_t y);
void InputCursor(std::int32_t* x, std::int32_t* y);
ImportAddress ResolveWindowImport(std::string_view dll, std::string_view symbol);
}
