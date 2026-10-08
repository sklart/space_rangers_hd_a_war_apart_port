#include "aft_font.hpp"

#include "rle_validation.hpp"

#include <algorithm>
#include <limits>
#include <new>

namespace srhd_awa::platform::aft_font {
namespace {
constexpr std::size_t kHeader = 32, kGlyph = 64, kPlane = 24, kRleHeader = 16;
constexpr std::size_t kMaxSource = 256u * 1024u * 1024u;
constexpr std::size_t kMaxGlyphs = 65536, kMaxPlane = 16u * 1024u * 1024u;
constexpr std::int32_t kMaxDimension = 8192;
std::uint32_t U32(const std::uint8_t* p) {
  return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
         (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
std::int32_t I32(const std::uint8_t* p) { return static_cast<std::int32_t>(U32(p)); }
bool Fail(std::string* error, const char* why) { if (error) *error = why; return false; }
bool PlaneValid(const std::uint8_t* source, std::size_t source_size, const GlyphPlane& plane,
                std::size_t literal_bytes) {
  if (plane.width < 0 || plane.height < 0 || plane.width > kMaxDimension || plane.height > kMaxDimension ||
      std::int64_t(plane.left) + plane.width > INT32_MAX ||
      std::int64_t(plane.left) + plane.width < INT32_MIN ||
      std::int64_t(plane.top) + plane.height > INT32_MAX ||
      std::int64_t(plane.top) + plane.height < INT32_MIN) return false;
  if (plane.data_offset == 0) return plane.data_size == 0;
  if (plane.width == 0 || plane.height == 0 || plane.data_size < kRleHeader ||
      plane.data_size > kMaxPlane || plane.data_offset > source_size ||
      plane.data_size > source_size - plane.data_offset) return false;
  return rle_validation::ValidateBuffer(source + plane.data_offset, plane.data_size,
                                        plane.width, plane.height, literal_bytes);
}
GlyphPlane ReadPlane(const std::uint8_t* p) {
  return {I32(p), I32(p + 4), I32(p + 8), I32(p + 12), U32(p + 16), U32(p + 20)};
}
struct Hash {
  std::uint32_t crc{0xffffffffu};
  std::uint64_t fnv{0xcbf29ce484222325ull};
  void Byte(std::uint8_t b) {
    crc ^= b;
    for (unsigned i = 0; i != 8; ++i) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    fnv = (fnv ^ b) * 0x100000001b3ull;
  }
  void Word(std::uint32_t n) { for (unsigned i = 0; i != 4; ++i) Byte(static_cast<std::uint8_t>(n >> (8 * i))); }
  void Plane(const GlyphPlane& p) {
    Word(static_cast<std::uint32_t>(p.left)); Word(static_cast<std::uint32_t>(p.top));
    Word(static_cast<std::uint32_t>(p.width)); Word(static_cast<std::uint32_t>(p.height));
    Word(p.data_offset); Word(p.data_size);
  }
  Fingerprint Finish() const { return {crc ^ 0xffffffffu, fnv}; }
};
}  // namespace

void AftFont::Clear() {
  bytes_.clear(); glyphs_.clear(); lookup_.fill(-1);
  version_ = centering_height_ = line_height_ = 0;
  above_baseline_ = below_baseline_ = max_glyph_advance_ = 0;
  duplicate_count_ = 0; fingerprint_ = {};
}

bool AftFont::Load(const std::uint8_t* source, std::size_t size, std::string* error) {
  Clear();
  if (!source || size < kHeader || size > kMaxSource) return Fail(error, "invalid AFT source size");
  if (source[0] != 'a' || source[1] != 'f' || source[2] != 't') return Fail(error, "invalid AFT magic");
  const auto version = I32(source + 4), count = I32(source + 8);
  if (version != 1) return Fail(error, "unsupported AFT version");
  if (I32(source + 20) > INT32_MAX - 2) return Fail(error, "AFT line height overflow");
  if (count < 0 || static_cast<std::size_t>(count) > kMaxGlyphs ||
      static_cast<std::size_t>(count) > (size - kHeader) / kGlyph) return Fail(error, "invalid AFT glyph table");
  try {
    std::vector<Glyph> parsed;
    parsed.reserve(static_cast<std::size_t>(count));
    std::array<std::int32_t, 65536> lookup;
    lookup.fill(-1);
    std::int32_t above = 0, below = 0, maximum = 0;
    std::size_t duplicates = 0;
    for (std::int32_t i = 0; i < count; ++i) {
      const auto* p = source + kHeader + static_cast<std::size_t>(i) * kGlyph;
      const auto code = U32(p);
      if (code > 0xffff) return Fail(error, "AFT glyph is outside UTF-16 BMP");
      Glyph g{static_cast<char32_t>(code), I32(p + 4), I32(p + 8), I32(p + 12),
              ReadPlane(p + 16), ReadPlane(p + 16 + kPlane)};
      if (!PlaneValid(source, size, g.opaque, 0) || !PlaneValid(source, size, g.alpha, 1))
        return Fail(error, "invalid AFT glyph plane or RLE");
      const std::int64_t advance = std::int64_t(g.advance_a) + g.advance_b + g.advance_c;
      if (advance < INT32_MIN || advance > INT32_MAX) return Fail(error, "AFT glyph advance overflow");
      maximum = std::max(maximum, static_cast<std::int32_t>(advance));
      for (const auto& plane : {g.opaque, g.alpha}) if (plane.data_offset) {
        if (plane.top <= 0) {
          const auto candidate = -std::int64_t(plane.top) + 1;
          if (candidate > INT32_MAX) return Fail(error, "AFT baseline overflow");
          above = std::max(above, static_cast<std::int32_t>(candidate));
        }
        const auto bottom = std::int64_t(plane.top) + plane.height - 1;
        if (bottom > 0) below = std::max(below, static_cast<std::int32_t>(bottom));
      }
      if (lookup[code] >= 0) ++duplicates;
      lookup[code] = i; // Upstream overwrites the lookup slot: the last duplicate wins.
      parsed.push_back(g);
    }
    Hash hash;
    hash.Word(static_cast<std::uint32_t>(version)); hash.Word(static_cast<std::uint32_t>(count));
    hash.Word(U32(source + 12)); hash.Word(U32(source + 20) + 2);
    hash.Word(static_cast<std::uint32_t>(above)); hash.Word(static_cast<std::uint32_t>(below));
    hash.Word(static_cast<std::uint32_t>(maximum));
    for (const auto& g : parsed) {
      hash.Word(static_cast<std::uint32_t>(g.char_code));
      hash.Word(static_cast<std::uint32_t>(g.advance_a)); hash.Word(static_cast<std::uint32_t>(g.advance_b));
      hash.Word(static_cast<std::uint32_t>(g.advance_c)); hash.Plane(g.opaque); hash.Plane(g.alpha);
    }
    bytes_.assign(source, source + size);
    glyphs_ = std::move(parsed); lookup_ = lookup;
    version_ = version; centering_height_ = I32(source + 12);
    line_height_ = I32(source + 20) + 2;
    above_baseline_ = above; below_baseline_ = below; max_glyph_advance_ = maximum;
    duplicate_count_ = duplicates; fingerprint_ = hash.Finish();
    if (error) error->clear();
    return true;
  } catch (const std::bad_alloc&) { Clear(); return Fail(error, "AFT allocation failed"); }
}

const Glyph* AftFont::Find(char16_t code) const {
  const auto index = lookup_[code];
  return index < 0 ? nullptr : &glyphs_[static_cast<std::size_t>(index)];
}
const std::uint8_t* AftFont::PlaneData(const GlyphPlane& plane) const {
  return plane.data_offset ? bytes_.data() + plane.data_offset : nullptr;
}
std::int32_t AftFont::Advance(char16_t code) const {
  const auto* g = Find(code);
  return g ? g->advance_a + g->advance_b + g->advance_c : 0;
}

}  // namespace srhd_awa::platform::aft_font
