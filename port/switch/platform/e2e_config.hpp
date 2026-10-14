#pragma once

#include <string>

namespace srhd_awa::platform::e2e_config {
// Resolves the shipped template and writable CFG independently. Never replaces
// an existing user file. detail contains paths and filesystem diagnostics.
bool EnsureUserConfig(std::string* detail);
}
