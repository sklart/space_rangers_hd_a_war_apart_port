#pragma once

#include "ui_controls_fingerprint.hpp"
#include "ui_tree_renderer.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace srhd_awa::platform::aft_font { class AftFont; }
namespace srhd_awa::platform::ui {
class UiGraphButton;
class UiWindow;
class UiZone;
}

namespace srhd_awa::platform::ui_controls_checkpoint {

struct Evidence {
  ui_fingerprint::Value tree, window_layout, graph_state, zone_hits, frame;
};

// Diagnostic-only M24 scene. Its resources are embedded synthetic bytes;
// the M23 release-font checkpoint runs separately and first.
class Checkpoint {
 public:
  bool Initialize(std::string* error = nullptr);
  bool VerifyFixed(Evidence* evidence, std::string* error = nullptr) const;
  bool StartDynamic(std::string* error = nullptr);
  bool Update(std::uint64_t now_ms, std::string* error = nullptr);
  bool Render(const scene_compositor::Framebuffer& target, std::string* error = nullptr) const;
  bool Rendered() const { return rendered_; }
  void MarkRendered() { rendered_ = true; }

 private:
  ui::UiTree tree_;
  std::shared_ptr<const aft_font::AftFont> font_;
  ui::UiWindow* window_{};
  ui::UiGraphButton* button_{};
  ui::UiZone* zone_{};
  bool initialized_{}, dynamic_{}, rendered_{};
  std::uint64_t last_tick_{};
};

}  // namespace srhd_awa::platform::ui_controls_checkpoint
