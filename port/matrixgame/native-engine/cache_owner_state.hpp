// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace srhd_awa::matrixgame {
class NativeInitRuntime;
namespace cache_owner_detail {
// Single owner slot shared with the existing Begin/Deinit state machine.
NativeInitRuntime*& Owner()noexcept;
NativeInitRuntime*& OriginalGameOwner()noexcept;
bool& OriginalGameInitialized()noexcept;
}
}
