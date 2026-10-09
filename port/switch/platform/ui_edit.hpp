#pragma once

#include "aft_font.hpp"
#include "image_object.hpp"
#include "scene_compositor.hpp"
#include "ui_object.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace srhd_awa::platform::ui {

enum class EditAlignX { Left, Center };

class UiEdit final : public UiObject {
 public:
  NodeKind Kind() const override { return NodeKind::Edit; }
  void SetFont(std::string key, std::shared_ptr<const aft_font::AftFont> font);
  void SetText(std::u16string value);
  void SetTextColor(std::uint16_t value) { text_color_ = value; }
  void SetCaretColor(std::uint16_t value) { caret_color_ = value; }
  void SetBorder(bool enabled, std::uint16_t light, std::uint16_t dark) {
    border_enabled_ = enabled; border_light_ = light; border_dark_ = dark;
  }
  void SetBackground(image_object::PortableImageObject image) {
    background_ = std::make_unique<image_object::PortableImageObject>(std::move(image));
  }
  void SetMaxLength(std::int32_t value) { max_length_ = value; }
  void SetAlignX(EditAlignX value) { align_x_ = value; }
  void SetClearFocusOnEnter(bool value) { clear_focus_on_enter_ = value; }
  void SetAutoScrollText(bool value) { auto_scroll_text_ = value; }
  void SetFocused(bool value);
  void SetCaretBlink(bool value) { caret_blink_on_ = value; }
  void SetCaretPosition(std::int32_t value);
  void MoveLeft();
  void MoveRight();
  void Home();
  void End();
  void Backspace();
  void Delete();
  bool InsertCharacter(char16_t value);
  bool RenderLeaf(const scene_compositor::Framebuffer& target, Rect clip,
                  std::string* error = nullptr) const override;
  Point TextStart(std::string* error = nullptr) const;
  std::int32_t FirstCharacter() const { return first_character_; }

  const std::string& FontKey() const { return font_key_; }
  const std::u16string& Text() const { return text_; }
  std::uint16_t TextColor() const { return text_color_; }
  std::uint16_t CaretColor() const { return caret_color_; }
  bool BorderEnabled() const { return border_enabled_; }
  std::uint16_t BorderLight() const { return border_light_; }
  std::uint16_t BorderDark() const { return border_dark_; }
  std::int32_t MaxLength() const { return max_length_; }
  std::int32_t CaretPosition() const { return caret_position_; }
  bool Focused() const { return focused_; }
  bool CaretBlinkOn() const { return caret_blink_on_; }
  bool AutoScrollText() const { return auto_scroll_text_; }
  bool ClearFocusOnEnter() const { return clear_focus_on_enter_; }
  EditAlignX AlignX() const { return align_x_; }

 private:
  std::string font_key_;
  std::shared_ptr<const aft_font::AftFont> font_;
  std::unique_ptr<image_object::PortableImageObject> background_;
  std::u16string text_;
  std::uint16_t text_color_{0xffff};
  std::uint16_t caret_color_{0xf800};
  std::uint16_t border_light_{0xffff};
  std::uint16_t border_dark_{0x31a6};
  std::int32_t max_length_{256}, caret_position_{};
  mutable std::int32_t first_character_{};
  EditAlignX align_x_{EditAlignX::Left};
  bool border_enabled_{}, auto_scroll_text_{}, focused_{}, caret_blink_on_{};
  bool clear_focus_on_enter_{true};
};

}  // namespace srhd_awa::platform::ui
