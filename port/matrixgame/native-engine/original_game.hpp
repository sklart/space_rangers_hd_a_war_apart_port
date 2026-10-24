// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
struct SRobotsSettings;
namespace srhd_awa::matrixgame {
struct OriginalGameOptions {
    void* module{};void* window{};
    const wchar_t* map{};std::uint32_t seed{};
    const SRobotsSettings* settings{};
    const wchar_t* language{};const wchar_t* start_text{};
    const wchar_t* win_text{};const wchar_t* loss_text{};const wchar_t* planet{};
    platform3g::Window* window_provider{};
    platform3g::Device* device_provider{};
};
namespace original_game {
void ValidateEntry();
void ValidateDeinit();
void ObserveStatics();
}
}
