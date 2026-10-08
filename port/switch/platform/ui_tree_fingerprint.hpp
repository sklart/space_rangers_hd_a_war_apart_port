#pragma once

#include "scene_compositor.hpp"
#include "ui_object.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace srhd_awa::platform::ui_fingerprint {

struct Value {
  std::uint32_t crc32{};
  std::uint64_t fnv64{};
  std::size_t bytes{};
};

// Canonical preorder serialization used only for portable M22 diagnostics and
// oracle comparison. Numeric fields are fixed-width little-endian.
bool ComputeTree(const ui::UiObject& root, Value* result, std::string* error = nullptr);
bool ComputeFramebuffer(const scene_compositor::Framebuffer& framebuffer, Value* result,
                        std::string* error = nullptr);

}  // namespace srhd_awa::platform::ui_fingerprint
