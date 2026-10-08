#pragma once

#include "ui_object.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace srhd_awa::platform::ui {

class UiImageLeaf;

enum class WindowSlot : std::uint8_t {
  TopLeft, TopRight, BottomLeft, BottomRight,
  Left, Right, Top, Bottom, Texture, Count
};

// The border is nine ordinary image children; configured controls remain
// children of the Panel and participate in its scroll, clip and depth rules.
class UiWindow final : public UiPanel {
 public:
  NodeKind Kind() const override { return NodeKind::Window; }
  bool AddBorderImage(WindowSlot slot, std::unique_ptr<UiImageLeaf> image,
                      std::string* error = nullptr);
  UiImageLeaf* BorderImage(WindowSlot slot) const;
  void SetMinimumSize(Size value) { minimum_size_ = value; }
  Size MinimumSize() const { return minimum_size_; }
  void SetWorkSubRect(Rect value) { work_sub_rect_ = value; }
  Rect WorkSubRect() const { return work_sub_rect_; }
  bool AlignSizeToBorderTiles(Size requested, Size* aligned, std::string* error = nullptr) const;
  bool FinalizeLayout(std::string* error = nullptr);

 protected:
  void OnGeometryChanged() override;

 private:
  static constexpr std::size_t Index(WindowSlot slot) { return static_cast<std::size_t>(slot); }
  bool HasAllImages() const;
  void Layout();
  std::array<UiImageLeaf*, static_cast<std::size_t>(WindowSlot::Count)> images_{};
  Size minimum_size_{};
  Rect work_sub_rect_{};
};

}  // namespace srhd_awa::platform::ui
