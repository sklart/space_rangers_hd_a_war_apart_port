#include "ui_edit.hpp"

#include "font_renderer.hpp"
#include "tagged_text.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace srhd_awa::platform::ui {
namespace {
bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}
void Pixel(const scene_compositor::Framebuffer& target, Rect clip,
           std::int32_t x, std::int32_t y, std::uint16_t color) {
  if (x >= clip.left && x < clip.right && y >= clip.top && y < clip.bottom &&
      x >= 0 && y >= 0 && x < target.width && y < target.height)
    target.pixels[static_cast<std::size_t>(y) * target.pitch_pixels + x] = color;
}
}  // namespace

UiEdit* UiObject::AddEdit() {
  auto child = std::make_unique<UiEdit>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

void UiEdit::SetFont(std::string key, std::shared_ptr<const aft_font::AftFont> font) {
  font_key_ = std::move(key);
  font_ = std::move(font);
}
void UiEdit::SetText(std::u16string value) {
  if (text_ != value) {
    text_ = std::move(value);
    caret_position_ = 0;
    first_character_ = 0;
    Invalidate();
  }
}
void UiEdit::SetFocused(bool value) {
  if (focused_ == value) return;
  focused_ = value;
  if (value) caret_position_ = static_cast<std::int32_t>(text_.size());
  Invalidate();
}
void UiEdit::SetCaretPosition(std::int32_t value) {
  caret_position_ = std::clamp(value, 0, static_cast<std::int32_t>(text_.size()));
  Invalidate();
}
void UiEdit::MoveLeft() { SetCaretPosition(caret_position_ - 1); }
void UiEdit::MoveRight() { SetCaretPosition(caret_position_ + 1); }
void UiEdit::Home() { SetCaretPosition(0); }
void UiEdit::End() { SetCaretPosition(static_cast<std::int32_t>(text_.size())); }
void UiEdit::Backspace() {
  if (caret_position_ <= 0) return;
  text_.erase(static_cast<std::size_t>(caret_position_ - 1), 1);
  --caret_position_;
  Invalidate();
}
void UiEdit::Delete() {
  if (caret_position_ >= static_cast<std::int32_t>(text_.size())) return;
  text_.erase(static_cast<std::size_t>(caret_position_), 1);
  Invalidate();
}
bool UiEdit::InsertCharacter(char16_t value) {
  if (!font_ || !font_->Find(value) || max_length_ < 0 ||
      text_.size() >= static_cast<std::size_t>(max_length_) ||
      (accept_character_ && !accept_character_(value))) return false;
  text_.insert(text_.begin() + caret_position_, value);
  ++caret_position_;
  Invalidate();
  return true;
}

Point UiEdit::TextStart(std::string* error) const {
  if (!font_) {
    if (error) *error = "Edit font is unresolved";
    return {};
  }
  const auto bounds = HitTestBounds();
  std::int32_t x = bounds.left + 2;
  if (align_x_ == EditAlignX::Center) {
    tagged_text::Bounds measured{};
    if (!tagged_text::MeasureTaggedTextBounds(*font_, text_, &measured,
                                              nullptr, error)) return {};
    x = bounds.left + (bounds.right - bounds.left) / 2 -
        (measured.right - measured.left) / 2;
  }
  const auto y = (bounds.top + bounds.bottom) / 2 -
      (font_->above_baseline() + font_->below_baseline()) / 2 +
      font_->above_baseline();
  if (error) error->clear();
  return {x, y};
}

bool UiEdit::RenderLeaf(const scene_compositor::Framebuffer& target, Rect clip,
                        std::string* error) const {
  if (!target.pixels || target.width <= 0 || target.height <= 0 ||
      target.pitch_pixels < target.width || !font_)
    return Fail(error, "Edit target or font is invalid");
  const auto bounds = HitTestBounds();
  clip = Intersect(Intersect(clip, bounds), {0, 0, target.width, target.height});
  if (IsEmpty(clip)) return true;
  if (background_ && background_->loaded()) {
    if (!background_->DrawFramebufferAt(target.pixels, target.width, target.height,
        target.pitch_pixels, bounds.left, bounds.top, ClientSize().width,
        ClientSize().height, {clip.left, clip.top, clip.right, clip.bottom}, error))
      return false;
  }
  const auto start = TextStart(error);
  if (error && !error->empty()) return false;
  first_character_ = 0;
  const auto count = static_cast<std::int32_t>(text_.size());
  if (auto_scroll_text_) {
    const auto visible_width = static_cast<std::int64_t>(ClientSize().width) - 2;
    while (first_character_ < count) {
      std::int64_t width{};
      bool overflow{};
      for (std::int32_t index = first_character_; index < count; ++index) {
        width += font_->Advance(text_[index]);
        if (caret_position_ >= index && width >= visible_width) {
          overflow = true;
          break;
        }
      }
      if (!overflow) break;
      ++first_character_;
    }
  }
  font_renderer::Target font_target{target.pixels, target.width,
      target.height, target.pitch_pixels,
      {clip.left, clip.top, clip.right, clip.bottom}};
  std::int64_t x = start.x;
  for (std::int32_t index = first_character_; index <= count; ++index) {
    if (index < count) {
      if (x >= std::numeric_limits<std::int32_t>::min() &&
          x <= std::numeric_limits<std::int32_t>::max() &&
          !font_renderer::DrawGlyph(*font_, text_[index], font_target,
                                    static_cast<std::int32_t>(x), start.y,
                                    text_color_, error)) return false;
    }
    if (focused_ && caret_blink_on_ && caret_position_ == index) {
      const auto top = start.y - (font_->above_baseline() - 1);
      const auto length = font_->above_baseline() + font_->below_baseline();
      for (std::int32_t dy = 0; dy < length; ++dy) {
        Pixel(target, clip, static_cast<std::int32_t>(x), top + dy, caret_color_);
        Pixel(target, clip, static_cast<std::int32_t>(x + 1), top + dy, caret_color_);
      }
    }
    if (index < count) x += font_->Advance(text_[index]);
  }
  if (border_enabled_ && !IsEmpty(bounds)) {
    for (auto x = bounds.left; x < bounds.right; ++x) {
      Pixel(target, clip, x, bounds.top, border_light_);
      Pixel(target, clip, x, bounds.bottom - 1, border_dark_);
    }
    for (auto y = bounds.top; y < bounds.bottom; ++y) {
      Pixel(target, clip, bounds.left, y, border_light_);
      Pixel(target, clip, bounds.right - 1, y, border_dark_);
    }
  }
  if (error) error->clear();
  return true;
}

}  // namespace srhd_awa::platform::ui
