#pragma once

#include <string>

namespace srhd_awa::platform::runtime_settings_slice {

// Executes only the portable, configuration-bearing prefix of the release
// startup path. It reaches the core GlobalCache only; audio and global UI remain excluded.
using BeforeDerivedRuntimeStateHook = void (*)(void* context);

// The hook observes loaded DAT roots before derived state mutates CacheDataRoot.
bool Initialize(std::string* error, BeforeDerivedRuntimeStateHook before_derived = nullptr,
                void* hook_context = nullptr);
void Shutdown();

}  // namespace srhd_awa::platform::runtime_settings_slice
