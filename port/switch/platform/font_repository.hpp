#pragma once

#include "aft_font.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace EC_Cache { struct TCacheEC; }

namespace srhd_awa::platform::font_repository {

class IFontResolver {
 public:
  virtual ~IFontResolver() = default;
  virtual bool LoadFont(const std::string& key, std::vector<std::uint8_t>* bytes,
                        std::string* resolved_source, std::string* error) = 0;
};

class Repository {
 public:
  explicit Repository(IFontResolver* resolver) : resolver_(resolver) {}
  bool Acquire(const std::string& key, std::shared_ptr<const aft_font::AftFont>* font,
               std::string* resolved_source = nullptr, std::string* error = nullptr);
  std::size_t size() const { return entries_.size(); }

 private:
  struct Entry { std::shared_ptr<const aft_font::AftFont> font; std::string source; };
  IFontResolver* resolver_{};
  std::unordered_map<std::string, Entry> entries_;
};

class CacheFontResolver final : public IFontResolver {
 public:
  explicit CacheFontResolver(EC_Cache::TCacheEC* cache) : cache_(cache) {}
  bool LoadFont(const std::string& key, std::vector<std::uint8_t>* bytes,
                std::string* resolved_source, std::string* error) override;
 private:
  EC_Cache::TCacheEC* cache_{};
};

// Reads the current M13 GlobalsV font-name fields; no second alias registry.
std::string ResolveLabelAlias(const std::string& requested, bool smoothing_enabled);

}  // namespace srhd_awa::platform::font_repository
