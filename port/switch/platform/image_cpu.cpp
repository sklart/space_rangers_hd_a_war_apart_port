#include "image_cpu.hpp"

#include "image_okgf_bridge.hpp"

#include <limits>
#include <new>

namespace srhd_awa::platform::image_cpu {
namespace {

constexpr std::uint64_t kMaxDecodedBytes = 256ull * 1024ull * 1024ull;

bool Fail(Image* image, std::string* error, const char* message) {
  image->Clear();
  if (error) *error = message;
  return false;
}

std::int32_t BytesPerPixel(Format format) {
  switch (format) {
    case Format::RGB565: return 2;
    case Format::BGRA8888: return 4;
    case Format::RGB888: return 3;
    case Format::Gray8: return 1;
  }
  return 0;
}

void Masks(Format format, std::uint32_t* red, std::uint32_t* green,
           std::uint32_t* blue, std::uint32_t* alpha) {
  *alpha = 0;
  switch (format) {
    case Format::RGB565:
      *red = 0xf800u; *green = 0x07e0u; *blue = 0x001fu;
      break;
    case Format::BGRA8888:
      *red = 0x00ff0000u; *green = 0x0000ff00u; *blue = 0x000000ffu;
      *alpha = 0xff000000u;
      break;
    case Format::RGB888:
      *red = 0x000000ffu; *green = 0x0000ff00u; *blue = 0x00ff0000u;
      break;
    case Format::Gray8:
      *red = 0x000000ffu; *green = 0; *blue = 0;
      break;
  }
}

}  // namespace

void Image::Clear() {
  width = 0;
  height = 0;
  pitch = 0;
  pixels.clear();
  pixels.shrink_to_fit();
}

bool Decode(const std::uint8_t* source, std::int32_t source_size, Format format,
            Image* image, std::string* error) {
  if (!image) {
    if (error) *error = "null image destination";
    return false;
  }
  image->Clear();
  if (!source || source_size <= 0) return Fail(image, error, "invalid image source");
  const std::int32_t bytes_per_pixel = BytesPerPixel(format);
  if (bytes_per_pixel == 0) return Fail(image, error, "invalid image format");

  std::int32_t width{};
  std::int32_t height{};
  void* context = image_okgf_bridge::BeginRead(source, source_size, &width, &height);
  if (!context) return Fail(image, error, "OKGF could not start image decode");
  const auto wide_width = static_cast<std::uint64_t>(width);
  const auto wide_height = static_cast<std::uint64_t>(height);
  const auto wide_pitch = wide_width * static_cast<std::uint64_t>(bytes_per_pixel);
  const auto byte_count = wide_pitch * wide_height;
  if (width <= 0 || height <= 0 || wide_pitch > std::numeric_limits<std::int32_t>::max() ||
      byte_count > kMaxDecodedBytes || byte_count > std::numeric_limits<std::size_t>::max()) {
    // BeginImageRead owns a live context only until its paired Read call. Its
    // public guard has already rejected dimensions unsafe at four bytes/pixel;
    // this second guard covers every requested output representation.
    image_okgf_bridge::CancelRead(context);
    return Fail(image, error, "decoded image exceeds CPU limit");
  }

  try {
    image->pixels.resize(static_cast<std::size_t>(byte_count));
  } catch (const std::bad_alloc&) {
    image_okgf_bridge::CancelRead(context);
    return Fail(image, error, "decoded image allocation failed");
  }
  std::uint32_t red{};
  std::uint32_t green{};
  std::uint32_t blue{};
  std::uint32_t alpha{};
  Masks(format, &red, &green, &blue, &alpha);
  if (!image_okgf_bridge::Read(context, image->pixels.data(),
                               static_cast<std::int32_t>(wide_pitch), red, green, blue,
                               alpha, bytes_per_pixel))
    return Fail(image, error, "OKGF image decode failed");
  image->width = width;
  image->height = height;
  image->pitch = static_cast<std::int32_t>(wide_pitch);
  image->format = format;
  if (error) error->clear();
  return true;
}

}  // namespace srhd_awa::platform::image_cpu
