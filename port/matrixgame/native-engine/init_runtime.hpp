// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "cache_resources.hpp"
#include "buffer_resources.hpp"
#include <memory>
namespace srhd_awa::matrixgame {
struct Cpu3GState {void* module;void* window;bool bound;};
void L3GInitAsDLLCpuPhase(void* module,void* window);
void L3GDeinitCpuPhase() noexcept;
Cpu3GState InspectCpu3GState() noexcept;
// Owns the cache and its buffer context for the pre-device portion of engine
// startup. This is a prerequisite of CGame::Init, not its implementation.
class NativeInitRuntime {
public:
    NativeInitRuntime()=default;
    ~NativeInitRuntime();
    NativeInitRuntime(const NativeInitRuntime&)=delete;
    NativeInitRuntime& operator=(const NativeInitRuntime&)=delete;
    void Begin(void* module,void* window);
    void Deinit() noexcept;
    bool CpuReady() const noexcept {return cpu_owned_;}
    bool GpuReady() const noexcept {return false;} // No device operation was entered.
    static const char* FirstUnsupportedOperation() noexcept;
private:
    std::unique_ptr<buffers::Context> buffers_;
    std::unique_ptr<buffers::Scope> scope_;
    bool cache_owned_{},cpu_owned_{};
};
}
