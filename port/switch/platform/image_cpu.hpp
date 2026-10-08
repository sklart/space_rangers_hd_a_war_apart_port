#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::image_cpu {

enum class Format { RGB565, BGRA8888, RGB888, Gray8 };

// CPU-only decoded image. The byte layout is exactly the layout requested
// from OKGF: RGB565 little-endian words, BGRA, RGB, or one gray byte.
struct Image {
  std::int32_t width{};
  std::int32_t height{};
  std::int32_t pitch{};
  Format format{Format::RGB565};
  std::vector<std::uint8_t> pixels;

  void Clear();
  bool empty() const { return pixels.empty(); }
};

bool Decode(const std::uint8_t* source, std::int32_t source_size, Format format,
            Image* image, std::string* error = nullptr);

}  // namespace srhd_awa::platform::image_cpu
