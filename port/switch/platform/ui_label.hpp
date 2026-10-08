#pragma once

#include "aft_font.hpp"
#include "image_object.hpp"
#include "tagged_text.hpp"
#include "ui_object.hpp"

#include <memory>
#include <string>
#include <vector>

namespace srhd_awa::platform::ui {

enum class LabelAlignX { Left, Center, Right, Auto };
enum class LabelAlignY { Top, Center, CenterEx, Bottom, Auto };

class UiLabelLeaf final : public UiObject {
 public:
  NodeKind Kind() const override { return NodeKind::LabelLeaf; }
  void SetFont(std::string key, std::shared_ptr<const aft_font::AftFont> font);
  void SetTextLines(std::vector<std::u16string> lines);
  void SetTextColor(std::uint16_t color) { text_color_ = color; dirty_ = true; }
  void SetAlignX(LabelAlignX value) { align_x_ = value; dirty_ = true; }
  void SetAlignY(LabelAlignY value) { align_y_ = value; dirty_ = true; }
  void SetWordWrap(bool value) { word_wrap_ = value; dirty_ = true; }
  void SetTextBorder(std::int32_t width, std::uint16_t color) { text_border_width_ = width; text_border_color_ = color; dirty_ = true; }
  void SetTextShadow(std::int32_t offset, std::uint16_t color) { text_shadow_offset_ = offset; text_shadow_color_ = color; dirty_ = true; }
  void SetBorder(bool enabled, std::uint16_t light, std::uint16_t dark) { border_enabled_ = enabled; border_light_color_ = light; border_dark_color_ = dark; }
  void SetAutoHeightPadding(std::int32_t value) { auto_height_padding_ = value; dirty_ = true; }
  void SetBackground(image_object::PortableImageObject image) { background_ = std::make_unique<image_object::PortableImageObject>(std::move(image)); }
  void SetSize(Size size) { UiObject::SetSize(size); dirty_ = true; }
  bool Prepare(std::string* error = nullptr) const;
  bool Render(const scene_compositor::Framebuffer& target, Rect clip, std::string* error = nullptr) const;
  bool RenderLeaf(const scene_compositor::Framebuffer& target, Rect clip,
                  std::string* error = nullptr) const override { return Render(target, clip, error); }

  const std::string& FontKey() const { return font_key_; }
  const std::vector<std::u16string>& TextLines() const { return text_lines_; }
  LabelAlignX AlignX() const { return align_x_; }
  LabelAlignY AlignY() const { return align_y_; }
  bool WordWrap() const { return word_wrap_; }
  std::uint16_t TextColor() const { return text_color_; }
  std::int32_t TextBorderWidth() const { return text_border_width_; }
  std::int32_t TextShadowOffset() const { return text_shadow_offset_; }
  bool BorderEnabled() const { return border_enabled_; }
  tagged_text::Bounds ContentBounds() const { return content_bounds_; }
  Size ContentSize() const { return content_size_; }
  std::size_t RenderedLineCount() const { return prepared_lines_.size(); }

 private:
  struct PreparedLine {
    std::u16string text;
    tagged_text::Layout layout;
    tagged_text::Bounds measure;
  };
  bool DrawLine(const PreparedLine& line, const scene_compositor::Framebuffer& target,
                font_renderer::Target font_target, std::int32_t x, std::int32_t y,
                std::int32_t dx, std::int32_t dy, bool use_tagged_color,
                std::uint16_t solid_color, bool justify,
                std::int32_t justify_width, std::string* error) const;
  std::string font_key_;
  std::shared_ptr<const aft_font::AftFont> font_;
  std::vector<std::u16string> text_lines_;
  std::unique_ptr<image_object::PortableImageObject> background_;
  std::uint16_t text_color_{0xffff};
  std::uint16_t text_border_color_{};
  std::uint16_t text_shadow_color_{};
  std::uint16_t border_light_color_{0xffff};
  std::uint16_t border_dark_color_{tagged_text::PackRgb565(55, 55, 55)};
  std::int32_t text_border_width_{}, text_shadow_offset_{}, auto_height_padding_{4};
  bool border_enabled_{}, word_wrap_{};
  LabelAlignX align_x_{LabelAlignX::Center};
  LabelAlignY align_y_{LabelAlignY::Center};
  mutable bool dirty_{true};
  mutable Size prepared_size_{};
  mutable std::vector<PreparedLine> prepared_lines_;
  mutable tagged_text::Bounds content_bounds_{};
  mutable Size content_size_{};
  mutable std::int32_t top_adjustment_{};
};

}  // namespace srhd_awa::platform::ui
