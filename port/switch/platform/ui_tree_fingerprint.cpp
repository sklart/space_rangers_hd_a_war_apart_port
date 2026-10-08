#include "ui_tree_fingerprint.hpp"

#include <bit>
#include <vector>

namespace srhd_awa::platform::ui_fingerprint {
namespace {
bool Fail(std::string* error, const char* message) { if (error) *error = message; return false; }
void U8(std::vector<std::uint8_t>* bytes, std::uint8_t value) { bytes->push_back(value); }
void U32(std::vector<std::uint8_t>* bytes, std::uint32_t value) { for (unsigned i{}; i != 4; ++i) U8(bytes, static_cast<std::uint8_t>(value >> (i * 8))); }
void U64(std::vector<std::uint8_t>* bytes, std::uint64_t value) { for (unsigned i{}; i != 8; ++i) U8(bytes, static_cast<std::uint8_t>(value >> (i * 8))); }
void I32(std::vector<std::uint8_t>* bytes, std::int32_t value) { U32(bytes, static_cast<std::uint32_t>(value)); }
std::uint32_t Crc32(const std::uint8_t* bytes, std::size_t size) {
  std::uint32_t value = 0xffffffffu;
  for (std::size_t i{}; i != size; ++i) { value ^= bytes[i]; for (unsigned bit{}; bit != 8; ++bit) value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1u))); }
  return value ^ 0xffffffffu;
}
std::uint64_t Fnv64(const std::uint8_t* bytes, std::size_t size) {
  std::uint64_t value = 0xcbf29ce484222325ull;
  for (std::size_t i{}; i != size; ++i) value = (value ^ bytes[i]) * 0x100000001b3ull;
  return value;
}
void Node(const ui::UiObject& node, std::int32_t parent_index, std::int32_t* next_index, std::vector<std::uint8_t>* bytes) {
  const std::int32_t own_index = (*next_index)++;
  U8(bytes, static_cast<std::uint8_t>(node.Kind())); U32(bytes, static_cast<std::uint32_t>(node.Name().size()));
  bytes->insert(bytes->end(), node.Name().begin(), node.Name().end()); I32(bytes, parent_index);
  const auto local = node.LocalPosition(); const auto absolute = node.AbsolutePosition();
  const auto size = node.ClientSize(); const auto origin = node.Origin();
  I32(bytes, local.x); I32(bytes, local.y); I32(bytes, absolute.x); I32(bytes, absolute.y);
  I32(bytes, size.width); I32(bytes, size.height); I32(bytes, origin.x); I32(bytes, origin.y);
  U64(bytes, std::bit_cast<std::uint64_t>(node.Depth())); U8(bytes, node.PositionModeW()); U8(bytes, node.Active()); U8(bytes, node.HitTestDisabled());
  U32(bytes, static_cast<std::uint32_t>(node.ChildCount()));
  for (const auto& child : node.Children()) Node(*child, own_index, next_index, bytes);
}
bool Finish(const std::vector<std::uint8_t>& bytes, Value* result, std::string* error) {
  if (!result) return Fail(error, "UI fingerprint result is null");
  result->bytes = bytes.size(); result->crc32 = Crc32(bytes.data(), bytes.size()); result->fnv64 = Fnv64(bytes.data(), bytes.size()); return true;
}
}  // namespace

bool ComputeTree(const ui::UiObject& root, Value* result, std::string* error) {
  std::vector<std::uint8_t> bytes; std::int32_t next_index{}; Node(root, -1, &next_index, &bytes); return Finish(bytes, result, error);
}
bool ComputeFramebuffer(const scene_compositor::Framebuffer& framebuffer, Value* result, std::string* error) {
  if (!framebuffer.pixels || framebuffer.width <= 0 || framebuffer.height <= 0 || framebuffer.pitch_pixels < framebuffer.width)
    return Fail(error, "UI fingerprint framebuffer is invalid");
  std::vector<std::uint8_t> bytes; bytes.reserve(static_cast<std::size_t>(framebuffer.width) * framebuffer.height * 2);
  for (std::int32_t y{}; y != framebuffer.height; ++y) for (std::int32_t x{}; x != framebuffer.width; ++x) {
    const auto pixel = framebuffer.pixels[y * framebuffer.pitch_pixels + x]; U8(&bytes, static_cast<std::uint8_t>(pixel)); U8(&bytes, static_cast<std::uint8_t>(pixel >> 8));
  }
  return Finish(bytes, result, error);
}

}  // namespace srhd_awa::platform::ui_fingerprint
