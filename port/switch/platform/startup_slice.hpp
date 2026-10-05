#pragma once

#include "runtime_platform.hpp"

#include <string>

namespace srhd_awa::platform::startup_slice {

struct State {
  runtime_platform::State platform;
  bool package_collection_initialized{};
};

bool Initialize(State* state, const std::string& game_root, const std::string& user_root,
                const std::string& startup_log_path,
                std::string* error);
void Shutdown(State* state);

}  // namespace srhd_awa::platform::startup_slice
