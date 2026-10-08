#pragma once

#include "ui_config.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace EC_Data { struct TDataEC; }

namespace srhd_awa::platform::ui_cache_resolver {

// Resolves Main.dat CacheData keys to raw bytes through the portable
// package/file adapters. This class never invokes an original GI/GAI decoder.
class CacheUiResourceResolver final : public ui_config::IUiResourceResolver {
 public:
  explicit CacheUiResourceResolver(EC_Data::TDataEC* root) : root_(root) {}
  bool LoadImage(ui::UiImageLeaf* leaf, image_object::Kind kind,
                 const std::string& resource, const std::string& option,
                 std::string* error) override;
  bool LoadGai(ui::UiGaiLeaf* leaf, const std::string& resource,
               std::string* error) override;

 private:
  bool Read(const std::string& key, std::vector<std::uint8_t>* bytes,
            std::size_t limit, std::string* error);
  EC_Data::TDataEC* root_{};
};

}  // namespace srhd_awa::platform::ui_cache_resolver
