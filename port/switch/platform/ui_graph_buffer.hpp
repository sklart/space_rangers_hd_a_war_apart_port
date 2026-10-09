#pragma once

#include "graph_buffer_cpu.hpp"
#include "ui_object.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace srhd_awa::platform::ui {

class UiGraphBuffer final : public UiObject {
 public:
  NodeKind Kind() const override { return NodeKind::GraphBuffer; }
  graph_buffer_cpu::GraphBufferCpu& Buffer() { return buffer_; }
  const graph_buffer_cpu::GraphBufferCpu& Buffer() const { return buffer_; }
  void SetModes(image_layout::XMode x, image_layout::YMode y) { x_mode_ = x; y_mode_ = y; }
  void SetHalfAlpha(bool value) { half_alpha_ = value; }
  bool HalfAlpha() const { return half_alpha_; }
  bool AllocateBuffer(std::int32_t width, std::int32_t height, std::string* error = nullptr) {
    return buffer_.AllocateBuffer(width, height, error);
  }
  void ClearOwnedBuffer() { buffer_.ClearOwnedBuffer(); }
  bool LoadBitmapBytes(const std::uint8_t* source, std::size_t size,
                       std::string* error = nullptr) {
    return buffer_.LoadBitmapBytes(source, size, ClientSize().width, ClientSize().height, error);
  }
  bool LoadGiBytes(const std::uint8_t* source, std::size_t size,
                   std::string* error = nullptr) {
    return buffer_.LoadGiBytes(source, size, ClientSize().width, ClientSize().height, error);
  }
  bool RenderLeaf(const scene_compositor::Framebuffer& target, Rect clip,
                  std::string* error = nullptr) const override;
  bool HitTestPixel(Point point) const;
  Point GetVisualCenter() const;

 private:
  graph_buffer_cpu::GraphBufferCpu buffer_;
  image_layout::XMode x_mode_{image_layout::XMode::Center};
  image_layout::YMode y_mode_{image_layout::YMode::Center};
  bool half_alpha_{};
};

}  // namespace srhd_awa::platform::ui
