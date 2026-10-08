#include "alpha_bitmap_cpu.hpp"

#include "image_cpu.hpp"

#include <limits>
#include <new>

namespace srhd_awa::platform::alpha_bitmap_cpu {
namespace {
constexpr std::size_t kHeaderBytes = 16;
constexpr std::uint64_t kMaxBytes = 256ull * 1024ull * 1024ull;

std::uint32_t Read32(const std::uint8_t* bytes) {
  return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
         (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}
bool ValidateRleInternal(const std::uint8_t* bytes, std::size_t byte_count, std::int32_t width,
                         std::int32_t height, std::size_t literal_bytes) {
  if (!bytes || byte_count < kHeaderBytes || width <= 0 || height <= 0 ||
      Read32(bytes + 4) != static_cast<std::uint32_t>(width) ||
      Read32(bytes + 8) != static_cast<std::uint32_t>(height)) return false;
  const auto stream_bytes = static_cast<std::size_t>(Read32(bytes));
  if (stream_bytes != byte_count - kHeaderBytes) return false;
  std::size_t at = kHeaderBytes;
  for (std::int32_t row = 0; row < height; ++row) {
    std::int32_t x = 0;
    for (;;) {
      if (at == byte_count) return false;
      const auto command = bytes[at++];
      if (command == 0) { if (x != width) return false; break; }
      if (command == 0x80) { if (x != 0) return false; break; }
      const auto count = static_cast<std::int32_t>(command & 0x7f);
      if (count <= 0 || count > width - x) return false;
      if (command & 0x80) {
        const auto count_bytes = static_cast<std::size_t>(count) * literal_bytes;
        if (count_bytes > byte_count - at) return false;
        at += count_bytes;
      }
      x += count;
    }
  }
  return at == byte_count;
}
bool Fail(std::string* error, const char* message) { if (error) *error = message; return false; }

template <class Builder>
bool Build(const image_cpu::Image& decoded, Builder builder, std::vector<std::uint8_t>* output) {
  const auto size = builder(decoded.pixels.data(), decoded.pitch, decoded.width, decoded.height, nullptr);
  if (size <= 0 || static_cast<std::uint64_t>(size) > kMaxBytes) return false;
  try { output->resize(static_cast<std::size_t>(size)); }
  catch (const std::bad_alloc&) { return false; }
  return builder(decoded.pixels.data(), decoded.pitch, decoded.width, decoded.height, output->data()) == size;
}
}

bool ValidateRle(const std::uint8_t* bytes, std::size_t byte_count, std::int32_t width,
                 std::int32_t height, std::size_t literal_bytes) {
  return ValidateRleInternal(bytes, byte_count, width, height, literal_bytes);
}

void AlphaBitmap::Clear() { width_ = 0; height_ = 0; trans_.clear(); trans_alpha_.clear(); alpha_.clear(); }

bool AlphaBitmap::Load(const std::uint8_t* source, std::int32_t source_size, std::string* error) {
  Clear();
  image_cpu::Image decoded;
  if (!image_cpu::Decode(source, source_size, image_cpu::Format::BGRA8888, &decoded, error)) return false;
  if (!Build(decoded, okgf_rle_bridge::BuildTransBufFromBgra, &trans_) ||
      !Build(decoded, okgf_rle_bridge::BuildTransAlphaBufFromBgra, &trans_alpha_) ||
      !Build(decoded, okgf_rle_bridge::BuildAlphaBufFromBgra, &alpha_) ||
      !ValidateRle(trans_.data(), trans_.size(), decoded.width, decoded.height, 2) ||
      !ValidateRle(trans_alpha_.data(), trans_alpha_.size(), decoded.width, decoded.height, 2) ||
      !ValidateRle(alpha_.data(), alpha_.size(), decoded.width, decoded.height, 1)) {
    Clear();
    return Fail(error, "invalid alpha bitmap RLE");
  }
  width_ = decoded.width;
  height_ = decoded.height;
  if (error) error->clear();
  return true;
}

bool AlphaBitmap::Draw(std::uint16_t* destination, std::int32_t destination_width,
                       std::int32_t destination_height, std::int32_t pitch_pixels,
                       std::int32_t x, std::int32_t y, const okgf_rle_bridge::Rect& clip,
                       std::string* error) const {
  if (!loaded() || !destination || destination_width <= 0 || destination_height <= 0 ||
      pitch_pixels < destination_width || clip.left < 0 || clip.top < 0 ||
      clip.right < clip.left || clip.bottom < clip.top || clip.right > destination_width ||
      clip.bottom > destination_height) return Fail(error, "invalid alpha bitmap draw");
  const auto pitch = pitch_pixels * static_cast<std::int32_t>(sizeof(std::uint16_t));
  // This is the original TCAlphaBitmapEC order; changing it changes translucent pixels.
  okgf_rle_bridge::DrawAlphaBuf565Clip(destination, pitch, x, y, alpha_.data(), clip);
  okgf_rle_bridge::DrawTransAlphaBuf565Clip(destination, pitch, x, y, trans_alpha_.data(), clip);
  okgf_rle_bridge::DrawTransBuf565Clip(destination, pitch, x, y, trans_.data(), clip);
  if (error) error->clear();
  return true;
}

}  // namespace srhd_awa::platform::alpha_bitmap_cpu
