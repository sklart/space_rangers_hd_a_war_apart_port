// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include <optional>
namespace srhd_awa::matrixgame::applet_window {
// libnx appletGetMessage first eventWaits with timeout 0. No signal is an
// ordinary empty poll; AM can also report an empty queue after the event race.
// The Switch TU checks these portable constants against installed SDK macros.
inline constexpr std::uint32_t MessageTimedOut=1u|(117u<<9);
inline constexpr std::uint32_t MessageQueueEmpty=128u|(3u<<9);
inline constexpr bool MessageUnavailable(std::uint32_t result)noexcept{
    const auto value=result&0x3fffffu; // libnx R_VALUE, including result flags.
    return value==MessageTimedOut || value==MessageQueueEmpty;
}
// nullopt means no message, true means continue, false means process requested
// exit. Never process the output message after an unsuccessful receive.
template<class Receive,class Process>
std::optional<bool> PollMessage(Receive&& receive,Process&& process){
    std::uint32_t message{};
    const std::uint32_t result=receive(&message);
    if(MessageUnavailable(result))return std::nullopt;
    platform3g::CheckDeviceResult(static_cast<platform3g::DeviceResult>(result),"appletGetMessage");
    return bool(process(message));
}
}
