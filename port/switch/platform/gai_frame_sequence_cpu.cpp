#include "gai_frame_sequence_cpu.hpp"

#include <charconv>
#include <cctype>
#include <limits>

namespace srhd_awa::platform::gai_frame_sequence_cpu {
namespace {
constexpr std::size_t kMaxSequenceFrames = 100000;
bool Fail(gai_cpu::GaiSequence* out, std::string* error, const char* reason) {
  if (out) out->Clear();
  if (error) *error = reason;
  return false;
}
void Spaces(std::string_view text, std::size_t* at) {
  while (*at < text.size() && std::isspace(static_cast<unsigned char>(text[*at]))) ++*at;
}
bool Number(std::string_view text, std::size_t* at, std::int32_t* value) {
  Spaces(text, at);
  if (*at == text.size() || !std::isdigit(static_cast<unsigned char>(text[*at]))) return false;
  const auto* begin = text.data() + *at;
  const auto* end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, *value);
  if (result.ec != std::errc{} || result.ptr == begin) return false;
  *at = static_cast<std::size_t>(result.ptr - text.data());
  Spaces(text, at);
  return true;
}
bool Symbol(std::string_view text, std::size_t* at, char expected) {
  Spaces(text, at);
  if (*at >= text.size() || text[*at] != expected) return false;
  ++*at;
  return true;
}
}  // namespace

bool Parse(std::string_view text, std::int32_t source_frame_count,
           gai_cpu::GaiSequence* out, std::string* error) {
  if (error) error->clear();
  if (!out || source_frame_count <= 0 || source_frame_count > 100000 || text.empty())
    return Fail(out, error, "invalid custom GAI sequence input");
  gai_cpu::GaiSequence parsed{};
  std::size_t at{};
  while (true) {
    Spaces(text, &at);
    if (at == text.size()) break;
    std::int32_t delay{}, first{}, last{};
    if (!Symbol(text, &at, '[') || !Number(text, &at, &delay) ||
        !Symbol(text, &at, ',') || !Number(text, &at, &first) ||
        !Symbol(text, &at, '-') || !Number(text, &at, &last) ||
        !Symbol(text, &at, ']'))
      return Fail(out, error, "malformed custom GAI sequence");
    if (delay <= 0 || first >= source_frame_count || last >= source_frame_count)
      return Fail(out, error, "custom GAI frame or delay is invalid");
    const auto count = static_cast<std::uint64_t>(first > last ? first - last : last - first) + 1;
    if (count > kMaxSequenceFrames - parsed.frames.size())
      return Fail(out, error, "custom GAI sequence exceeds limit");
    const auto step = first <= last ? 1 : -1;
    for (std::int32_t frame = first;; frame += step) {
      parsed.frames.push_back({frame, delay});
      if (frame == last) break;
    }
  }
  if (parsed.frames.empty()) return Fail(out, error, "empty custom GAI sequence");
  *out = std::move(parsed);
  return true;
}

}  // namespace srhd_awa::platform::gai_frame_sequence_cpu
