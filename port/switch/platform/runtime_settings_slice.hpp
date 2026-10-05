#pragma once

#include <string>

namespace srhd_awa::platform::runtime_settings_slice {

// Executes only the portable, configuration-bearing prefix of the release
// startup path.  It deliberately stops before GlobalCache, audio and global UI.
bool Initialize(std::string* error);
void Shutdown();

}  // namespace srhd_awa::platform::runtime_settings_slice
