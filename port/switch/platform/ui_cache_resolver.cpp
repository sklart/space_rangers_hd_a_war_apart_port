#include "ui_cache_resolver.hpp"

#include "ui_gai.hpp"
#include "ui_graph_buffer.hpp"
#include "ui_tree_renderer.hpp"
#include "types/EC_Buf.hpp"
#include "types/EC_Data.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_File.hpp"

#include <limits>

namespace srhd_awa::platform::ui_cache_resolver {
namespace {
constexpr std::size_t kGiGaiLimit = 256ull * 1024 * 1024;
bool Fail(std::string* error, const char* reason) {
  if (error) *error = reason;
  return false;
}
}

bool CacheUiResourceResolver::Read(const std::string& key,
                                    std::vector<std::uint8_t>* bytes,
                                    std::size_t limit, std::string* error) {
  if (!root_ || !bytes || key.empty())
    return Fail(error, "UI cache resolver is unavailable");
  bytes->clear();
  const pas::WideString wide_key(key.c_str());
  EC_Buf::TBufEC* buffer{};
  try {
    auto* entry = root_->FindEntryByPath(wide_key);
    if (!entry || entry->Kind != EC_Data::dekFile ||
        !root_->FileExistsByPath(wide_key) ||
        !entry->SharedFileRef || !entry->SharedFileRef->FileRef)
      return Fail(error, "UI cache key is unresolved");
    std::int64_t source_size = entry->ByteCount;
    if (source_size < 0) {
      const auto file_size = entry->SharedFileRef->FileRef->GetSize();
      if (file_size < entry->FileOffset)
        return Fail(error, "UI cache entry offset exceeds file size");
      source_size = static_cast<std::int64_t>(file_size) - entry->FileOffset;
    }
    if (source_size <= 0 || static_cast<std::uint64_t>(source_size) > limit)
      return Fail(error, "UI cache resource exceeds size limit");
    buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
    root_->ReadEntryBuffer(entry, buffer);
    if (!buffer || !buffer->Data || buffer->DataSize <= 0 ||
        static_cast<std::size_t>(buffer->DataSize) > limit) {
      pas::free(buffer);
      return Fail(error, "UI cached resource is empty or oversized");
    }
    const auto* data = static_cast<const std::uint8_t*>(buffer->Data);
    bytes->assign(data, data + buffer->DataSize);
    pas::free(buffer);
    if (error) error->clear();
    return true;
  } catch (...) {
    pas::free(buffer);
    bytes->clear();
    return Fail(error, "UI cache resource read failed");
  }
}

bool CacheUiResourceResolver::LoadImage(ui::UiImageLeaf* leaf, image_object::Kind kind,
                                        const std::string& resource, const std::string& option,
                                        std::string* error) {
  if (!leaf || kind == image_object::Kind::GAI)
    return Fail(error, "invalid UI image kind for cache resolver");
  std::vector<std::uint8_t> bytes;
  const auto limit = kind == image_object::Kind::GI ? kGiGaiLimit :
                     static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
  return Read(resource, &bytes, limit, error) &&
         leaf->LoadBytes(kind, bytes.data(), bytes.size(), resource, option, error);
}

bool CacheUiResourceResolver::LoadGai(ui::UiGaiLeaf* leaf,
                                      const std::string& resource, std::string* error) {
  if (!leaf) return Fail(error, "UI GAI leaf is null");
  std::vector<std::uint8_t> bytes;
  return Read(resource, &bytes, kGiGaiLimit, error) &&
         leaf->LoadBytes(bytes.data(), bytes.size(), resource, error);
}

bool CacheUiResourceResolver::LoadGraphBuffer(ui::UiGraphBuffer* leaf, bool gi,
                                                const std::string& resource,
                                                std::string* error) {
  if (!leaf) return Fail(error, "UI GraphBuf leaf is null");
  std::vector<std::uint8_t> bytes;
  return Read(resource, &bytes, kGiGaiLimit, error) &&
         (gi ? leaf->LoadGiBytes(bytes.data(), bytes.size(), error)
             : leaf->LoadBitmapBytes(bytes.data(), bytes.size(), error));
}

}  // namespace srhd_awa::platform::ui_cache_resolver
