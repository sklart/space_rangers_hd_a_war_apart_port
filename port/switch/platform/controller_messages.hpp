#pragma once
#include <cstdint>
namespace srhd_awa::platform::controller_messages {
inline constexpr std::uint32_t kMessage = 0x8515;
enum class Edge : std::int32_t { Pressed, Released, Repeat, FocusMode, PointerMode };
}
