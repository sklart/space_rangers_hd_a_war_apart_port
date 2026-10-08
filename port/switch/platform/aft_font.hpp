#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::aft_font {

struct GlyphPlane {
  std::int32_t left{}, top{}, width{}, height{};
  std::uint32_t data_offset{}, data_size{};
};

struct Glyph {
  char32_t char_code{};
  std::int32_t advance_a{}, advance_b{}, advance_c{};
  GlyphPlane opaque, alpha;
};

struct Fingerprint {
  std::uint32_t crc32{};
  std::uint64_t fnv64{};
};

class AftFont {
 public:
  AftFont() { lookup_.fill(-1); }
  bool Load(const std::uint8_t* source, std::size_t size, std::string* error = nullptr);
  void Clear();
  const Glyph* Find(char16_t code) const;
  const std::uint8_t* PlaneData(const GlyphPlane& plane) const;
  std::int32_t Advance(char16_t code) const;
  std::int32_t version() const { return version_; }
  std::int32_t centering_height() const { return centering_height_; }
  std::int32_t line_height() const { return line_height_; }
  std::int32_t above_baseline() const { return above_baseline_; }
  std::int32_t below_baseline() const { return below_baseline_; }
  std::int32_t max_glyph_advance() const { return max_glyph_advance_; }
  std::size_t duplicate_count() const { return duplicate_count_; }
  const std::vector<Glyph>& glyphs() const { return glyphs_; }
  Fingerprint fingerprint() const { return fingerprint_; }

 private:
  std::vector<std::uint8_t> bytes_;
  std::vector<Glyph> glyphs_;
  std::array<std::int32_t, 65536> lookup_{};
  std::int32_t version_{}, centering_height_{}, line_height_{};
  std::int32_t above_baseline_{}, below_baseline_{}, max_glyph_advance_{};
  std::size_t duplicate_count_{};
  Fingerprint fingerprint_{};
};

}  // namespace srhd_awa::platform::aft_font
