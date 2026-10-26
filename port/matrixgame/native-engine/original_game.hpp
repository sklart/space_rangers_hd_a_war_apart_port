// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
struct SRobotsSettings;
namespace Base {class CBlockPar;}
namespace srhd_awa::matrixgame {
struct NativeDrawSink;
struct NativeGameOptions {
    platform3g::Window& window;
    platform3g::Device& device;
    NativeDrawSink* draw_sink{};
    const wchar_t* map{};std::uint32_t seed{};
    const wchar_t* language{};const wchar_t* start_text{};
    const wchar_t* win_text{};const wchar_t* loss_text{};const wchar_t* planet{};
    const wchar_t* output_root{}; // Explicit private native cache/screenshots root; borrowed only during entry.
    const wchar_t* resource_root{}; // Existing read-only game tree; never changes cwd or write destinations.
};
struct OriginalGameOptions {
    void* module{};void* window{};
    const wchar_t* map{};std::uint32_t seed{};
    const SRobotsSettings* settings{};
    const wchar_t* language{};const wchar_t* start_text{};
    const wchar_t* win_text{};const wchar_t* loss_text{};const wchar_t* planet{};
    platform3g::Window* window_provider{};
    platform3g::Device* device_provider{};
    const wchar_t* resource_root{};
};
namespace original_game {
void EnterPlatform(void*,Base::CBlockPar&,const wchar_t*,const wchar_t*,void*,std::uintptr_t,std::uintptr_t);
void ValidateEntry();
void ValidateDeinit();
void ObserveStatics();
void ObserveStage(const char*);
}
}
