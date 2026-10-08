#pragma once

#include "ui_object.hpp"

#include <functional>
#include <string>

namespace EC_BlockPar { struct TBlockParEC; }

namespace srhd_awa::platform::ui_config {

using DepthResolver = std::function<bool(const std::string&, double*)>;

// The adapter owns no EC_BlockPar state.  UiObject remains renderable after
// the borrowed config tree has been released.
struct Context {
  EC_BlockPar::TBlockParEC* styles{};
  DepthResolver resolve_depth;
};

bool ApplyBaseProperties(ui::UiObject* object, EC_BlockPar::TBlockParEC* block,
                         const Context& context, std::string* error = nullptr);
bool ResolveRuntimeDepth(EC_BlockPar::TBlockParEC* depth_config, const std::string& name,
                         double* value);

}  // namespace srhd_awa::platform::ui_config
