#include "ui_label.hpp"

#include "scene_compositor.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <new>

namespace srhd_awa::platform::ui {
namespace {
bool Fail(std::string* error, const char* reason) { if (error) *error = reason; return false; }
bool Narrow(std::int64_t value, std::int32_t* out) {
  if (value < INT32_MIN || value > INT32_MAX) return false;
  *out = static_cast<std::int32_t>(value); return true;
}
void Pixel(const scene_compositor::Framebuffer& target, Rect clip,
           std::int32_t x, std::int32_t y, std::uint16_t color) {
  if (x >= clip.left && x < clip.right && y >= clip.top && y < clip.bottom &&
      x >= 0 && x < target.width && y >= 0 && y < target.height)
    target.pixels[static_cast<std::size_t>(y) * target.pitch_pixels + x] = color;
}
}  // namespace

UiLabelLeaf* UiObject::AddLabel() {
  auto child = std::make_unique<UiLabelLeaf>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

void UiLabelLeaf::SetFont(std::string key, std::shared_ptr<const aft_font::AftFont> font) {
  font_key_ = std::move(key); font_ = std::move(font); dirty_ = true;
}
void UiLabelLeaf::SetTextLines(std::vector<std::u16string> lines) {
  text_lines_ = std::move(lines); dirty_ = true;
}

bool UiLabelLeaf::Prepare(std::string* error) const {
  const auto size = ClientSize();
  if (!dirty_ && prepared_size_ == size) return true;
  prepared_lines_.clear(); content_bounds_ = {}; content_size_ = {}; top_adjustment_ = 0;
  if (!font_) {
    if (!text_lines_.empty()) return Fail(error, "Label text has no resolved AFT font");
    dirty_ = false; prepared_size_ = size; return true;
  }
  try {
    for (const auto& source : text_lines_) {
      std::vector<std::u16string> wrapped;
      if (word_wrap_) {
        if (size.width <= 4) return Fail(error, "Label word-wrap width is nonpositive");
        if (!tagged_text::WrapTaggedTextIntoLines(*font_, source, size.width - 4, &wrapped, error)) return false;
      } else wrapped.push_back(source);
      for (auto& text : wrapped) {
        PreparedLine line;
        line.text = std::move(text);
        if (!tagged_text::MeasureTaggedTextBounds(*font_, line.text, &line.measure,
                                                  prepared_lines_.empty() ? &top_adjustment_ : nullptr, error) ||
            !tagged_text::LayoutLine(*font_, line.text, {text_color_, true, true}, &line.layout, error))
          return false;
        prepared_lines_.push_back(std::move(line));
      }
    }
    if (!prepared_lines_.empty()) {
      auto first = prepared_lines_.front().measure;
      std::int64_t left = first.left, right = first.right, top = first.top, bottom = first.bottom;
      for (std::size_t i = 1; i < prepared_lines_.size(); ++i) {
        const auto& bounds = prepared_lines_[i].measure;
        const auto dy = std::int64_t(font_->line_height()) * i;
        left = std::min<std::int64_t>(left, bounds.left);
        right = std::max<std::int64_t>(right, bounds.right);
        top = std::min<std::int64_t>(top, bounds.top + dy);
        bottom = std::max<std::int64_t>(bottom, bounds.bottom + dy);
      }
      if (!Narrow(left, &content_bounds_.left) || !Narrow(right, &content_bounds_.right) ||
          !Narrow(top, &content_bounds_.top) || !Narrow(bottom, &content_bounds_.bottom))
        return Fail(error, "Label content bounds overflow");
    }
    const auto extra = std::int64_t(text_border_width_) +
                       std::max(text_border_width_, text_shadow_offset_);
    const auto width = std::int64_t(content_bounds_.right) - content_bounds_.left + extra;
    const auto height = std::int64_t(content_bounds_.bottom) - content_bounds_.top + extra + 2;
    if (!Narrow(width, &content_size_.width) || !Narrow(height, &content_size_.height))
      return Fail(error, "Label content size overflow");
    Size actual = size;
    if (align_x_ == LabelAlignX::Auto) {
      if (!Narrow(std::int64_t(content_size_.width) + 4, &actual.width))
        return Fail(error, "Label auto width overflow");
    }
    if (align_y_ == LabelAlignY::Auto) {
      if (!Narrow(std::int64_t(content_size_.height) + auto_height_padding_, &actual.height))
        return Fail(error, "Label auto height overflow");
    }
    if (actual != size) const_cast<UiLabelLeaf*>(this)->UiObject::SetSize(actual);
    prepared_size_ = actual;
    dirty_ = false;
    if (error) error->clear();
    return true;
  } catch (const std::bad_alloc&) {
    prepared_lines_.clear(); return Fail(error, "Label layout allocation failed");
  }
}

bool UiLabelLeaf::DrawLine(const PreparedLine& line, const scene_compositor::Framebuffer&,
                           font_renderer::Target font_target, std::int32_t x, std::int32_t y,
                           std::int32_t dx, std::int32_t dy, bool use_tagged_color,
                           std::uint16_t solid_color, bool justify,
                           std::int32_t justify_width, std::string* error) const {
  if (justify) {
    std::int32_t at_x{}, at_y{};
    if (!Narrow(std::int64_t(x) + dx, &at_x) || !Narrow(std::int64_t(y) + dy, &at_y))
      return Fail(error, "Label justified position overflow");
    return tagged_text::DrawJustifiedLayout(*font_, line.layout, font_target,
        at_x, at_y, justify_width, use_tagged_color, solid_color, error);
  }
  for (const auto& glyph : line.layout.glyphs) {
    std::int32_t gx{}, gy{};
    if (!Narrow(std::int64_t(x) + dx + glyph.x, &gx) ||
        !Narrow(std::int64_t(y) + dy, &gy)) return Fail(error, "Label glyph position overflow");
    if (!font_renderer::DrawGlyph(*font_, glyph.code, font_target, gx, gy,
                                  use_tagged_color ? glyph.color : solid_color, error)) return false;
  }
  return true;
}

bool UiLabelLeaf::Render(const scene_compositor::Framebuffer& target, Rect clip,
                         std::string* error) const {
  if (!target.pixels || target.width <= 0 || target.height <= 0 ||
      target.pitch_pixels < target.width) return Fail(error, "invalid Label target");
  if (!Prepare(error)) return false;
  clip = Intersect(clip, {0, 0, target.width, target.height});
  if (IsEmpty(clip)) return true;
  const auto bounds = HitTestBounds();
  if (background_ && background_->loaded()) {
    if (!background_->DrawFramebufferAt(target.pixels, target.width, target.height,
        target.pitch_pixels, bounds.left, bounds.top, ClientSize().width, ClientSize().height,
        {clip.left, clip.top, clip.right, clip.bottom}, error)) return false;
  }
  if (font_ && !prepared_lines_.empty()) {
    const auto size = ClientSize();
    const auto absolute = AbsolutePosition();
    std::int32_t text_left{};
    if (word_wrap_ || align_x_ == LabelAlignX::Left || align_x_ == LabelAlignX::Auto)
      text_left = absolute.x + 2;
    else if (align_x_ == LabelAlignX::Right)
      text_left = absolute.x + size.width - content_size_.width - 2;
    else text_left = absolute.x + size.width / 2 - content_size_.width / 2;
    std::int32_t text_top{};
    if (align_y_ == LabelAlignY::Top || align_y_ == LabelAlignY::Auto)
      text_top = absolute.y + 2 + top_adjustment_;
    else if (align_y_ == LabelAlignY::Bottom)
      text_top = absolute.y + size.height - content_size_.height - 2 + top_adjustment_;
    else text_top = absolute.y + size.height / 2 - content_size_.height / 2 + top_adjustment_;
    std::int32_t baseline = text_top + font_->above_baseline() - 2;
    if (align_y_ == LabelAlignY::CenterEx && !word_wrap_)
      baseline = bounds.top + size.height / 2 -
          (font_->line_height() * static_cast<std::int32_t>(prepared_lines_.size() - 1) +
           font_->centering_height()) / 2 + font_->centering_height();
    font_renderer::Target font_target{target.pixels, target.width, target.height,
                                      target.pitch_pixels,
                                      {clip.left, clip.top, clip.right, clip.bottom}};
    for (std::size_t i = 0; i < prepared_lines_.size(); ++i) {
      const auto& line = prepared_lines_[i];
      std::int32_t x = text_left;
      if (!word_wrap_ && align_x_ == LabelAlignX::Right)
        x = bounds.right - (line.measure.right - line.measure.left) - 2;
      else if (!word_wrap_ && align_x_ == LabelAlignX::Center)
        x = (bounds.right - bounds.left) / 2 + bounds.left -
            (line.measure.right - line.measure.left) / 2;
      const auto y64 = std::int64_t(baseline) + std::int64_t(i) * font_->line_height();
      std::int32_t y{};
      if (!Narrow(y64, &y)) return Fail(error, "Label line position overflow");
      const bool justify = align_x_ == LabelAlignX::Auto && i + 1 < prepared_lines_.size();
      const auto justify_width = size.width - 4;
      if (text_shadow_offset_ > 0 && !DrawLine(line, target, font_target, x, y,
          text_shadow_offset_, text_shadow_offset_, false, text_shadow_color_,
          justify, justify_width, error)) return false;
      if (text_border_width_ > 0) {
        for (const auto [dx, dy] : {Point{-text_border_width_, -text_border_width_},
                                     Point{text_border_width_, -text_border_width_},
                                     Point{-text_border_width_, text_border_width_},
                                     Point{text_border_width_, text_border_width_}})
          if (!DrawLine(line, target, font_target, x, y, dx, dy, false,
                        text_border_color_, justify, justify_width, error)) return false;
      }
      if (!DrawLine(line, target, font_target, x, y, 0, 0, true, text_color_,
                    justify, justify_width, error)) return false;
    }
  }
  if (border_enabled_ && !IsEmpty(bounds)) {
    for (std::int32_t x = bounds.left; x < bounds.right; ++x) {
      Pixel(target, clip, x, bounds.top, border_light_color_);
      Pixel(target, clip, x, bounds.bottom - 1, border_dark_color_);
    }
    for (std::int32_t y = bounds.top; y < bounds.bottom; ++y) {
      Pixel(target, clip, bounds.left, y, border_light_color_);
      Pixel(target, clip, bounds.right - 1, y, border_dark_color_);
    }
  }
  if (error) error->clear();
  return true;
}

}  // namespace srhd_awa::platform::ui
