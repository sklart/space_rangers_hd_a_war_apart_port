// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
namespace srhd_awa::matrixgame::platform3g {
struct Unsupported:std::runtime_error {using std::runtime_error::runtime_error;};
// Only facts actually supplied by a backend. No D3DCAPS9 impersonation or
// substitution of process RAM/staging budgets for available texture memory.
struct DeviceCaps {
    bool calibrate_gamma{};
    std::uint32_t max_texture_stages{},max_anisotropy{},swapchain_count{};
};
struct GammaRamp {std::array<std::uint16_t,256> red{},green{},blue{};};
class Device {
public:
    virtual ~Device()=default;
    virtual DeviceCaps ReadCaps()=0;
    virtual GammaRamp ReadGamma(std::uint32_t swapchain)=0;
    virtual void RestoreGamma(std::uint32_t swapchain,const GammaRamp&) noexcept=0;
    virtual std::uint64_t AvailableTextureBytes()=0;
};
enum class Visibility {Maximized,Restored,Minimized};
class Window;
using MessageHandler=std::intptr_t(*)(Window&,std::uint32_t,std::uintptr_t,std::intptr_t);
class WindowAttachment {
public:
    virtual ~WindowAttachment()=default;
    virtual void Restore() noexcept=0;
};
class Window {
public:
    virtual ~Window()=default;
    virtual void* Identity() const noexcept=0;
    virtual std::unique_ptr<WindowAttachment> Attach(MessageHandler)=0;
    virtual void Show(Visibility)=0;
    virtual void ReleaseCursor()=0;
    virtual std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t)=0;
};
struct State {
    void* module{};void* window_identity{};bool external{};
    Window* window{};Device* device{};
    std::unique_ptr<WindowAttachment> attachment;
    DeviceCaps caps{};
    std::array<GammaRamp,2> saved_gamma{};
    std::array<bool,2> gamma_saved{};
    std::uint32_t flags{};
    int available_texture_mib{};
    bool ready{};
};
State& Current() noexcept;
// Original DLL bootstrap and WndProc/input algorithms via typed backend calls.
// Only external/borrowed device/window mode is supported; no fake owned EXE.
void InitAsDLL(void* module,Window&,Device&);
void Deinit() noexcept;
std::intptr_t WindowProcedure(Window&,std::uint32_t,std::uintptr_t,std::intptr_t);
}
