// SPDX-License-Identifier: GPL-2.0-or-later
#include "cache_owner_state.hpp"
#include "init_runtime.hpp"
namespace srhd_awa::matrixgame {
namespace cache_owner_detail {
NativeInitRuntime*& Owner()noexcept {
    static NativeInitRuntime* active{};
    return active;
}
NativeInitRuntime*& OriginalGameOwner()noexcept {static NativeInitRuntime* owner{};return owner;}
bool& OriginalGameInitialized()noexcept {static bool initialized{};return initialized;}
}
bool NativeCacheFactoryBound()noexcept {
    auto* active=cache_owner_detail::Owner();
    return active && active->CpuReady();
}
}
