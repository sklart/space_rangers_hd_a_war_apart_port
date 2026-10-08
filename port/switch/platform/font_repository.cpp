#include "font_repository.hpp"

#include <new>

namespace srhd_awa::platform::font_repository {
namespace {
bool Fail(std::string* error, const char* why) { if (error) *error = why; return false; }
}
bool Repository::Acquire(const std::string& key, std::shared_ptr<const aft_font::AftFont>* font,
                         std::string* resolved_source, std::string* error) {
  if (!font || key.empty() || !resolver_) return Fail(error, "invalid AFT repository request");
  *font = {};
  if (const auto existing = entries_.find(key); existing != entries_.end()) {
    *font = existing->second.font;
    if (resolved_source) *resolved_source = existing->second.source;
    if (error) error->clear();
    return true;
  }
  try {
    std::vector<std::uint8_t> bytes;
    std::string source;
    if (!resolver_->LoadFont(key, &bytes, &source, error)) return false;
    auto parsed = std::make_shared<aft_font::AftFont>();
    if (!parsed->Load(bytes.data(), bytes.size(), error)) return false;
    auto [it, inserted] = entries_.emplace(key, Entry{parsed, source});
    *font = it->second.font;
    if (resolved_source) *resolved_source = it->second.source;
    if (error) error->clear();
    return true;
  } catch (const std::bad_alloc&) { return Fail(error, "AFT repository allocation failed"); }
}
}  // namespace srhd_awa::platform::font_repository
