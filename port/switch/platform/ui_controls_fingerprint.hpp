#pragma once

#include "ui_tree_fingerprint.hpp"

#include <vector>

namespace srhd_awa::platform::ui {
class UiGraphButton;
class UiWindow;
class UiZone;
}

namespace srhd_awa::platform::ui_controls_fingerprint {

using Value = ui_fingerprint::Value;

// Fixed-width little-endian records for the independent M24 oracle and
// cumulative hardware checkpoint.  The generic M22 tree hash is separate.
bool ComputeWindowLayout(const ui::UiWindow& window, Value* result);
bool ComputeGraphState(const ui::UiGraphButton& button, Value* result);
bool ComputeZoneHits(const ui::UiZone& zone, const std::vector<ui::Point>& points,
                     Value* result);

}  // namespace srhd_awa::platform::ui_controls_fingerprint
