#include "font_repository.hpp"

#include "types/EC_Buf.hpp"
#include "types/EC_Cache.hpp"
#include "units/EC_Cache.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_File.hpp"
#include "units/GR_Main.hpp"
#include "units/GlobalsV.hpp"

namespace srhd_awa::platform::font_repository {
namespace {
bool Fail(std::string* error, const char* why) { if (error) *error = why; return false; }
std::string Narrow(const pas::WideString& text) { return static_cast<pas::AnsiString>(text).c_str(); }
}
bool CacheFontResolver::LoadFont(const std::string& key, std::vector<std::uint8_t>* bytes,
                                 std::string* resolved_source, std::string* error) {
  if (!cache_ || !bytes || !resolved_source) return Fail(error, "invalid runtime font resolver");
  bytes->clear(); resolved_source->clear();
  if (!GR_Main::CacheDataRoot) return Fail(error, "font cache mapping is unavailable");
  const pas::WideString wide_key(key.c_str());
  auto* entry = GR_Main::CacheDataRoot->FindEntryByPath(wide_key);
  if (!entry || entry->Kind != EC_Data::dekFile ||
      !GR_Main::CacheDataRoot->FileExistsByPath(wide_key))
    return Fail(error, "unresolved font cache key");
  const auto source = entry->SharedFileRef && entry->SharedFileRef->FileRef
      ? Narrow(entry->SharedFileRef->FileRef->FileName) : std::string{};
  EC_Buf::TBufEC* buffer = nullptr;
  try {
    buffer = cache_->OpenDataBuffer(wide_key);
    if (!buffer || !buffer->Data || buffer->DataSize < 0) {
      pas::free(buffer); return Fail(error, "empty cached AFT source");
    }
    const auto* data = static_cast<const std::uint8_t*>(buffer->Data);
    bytes->assign(data, data + buffer->DataSize);
    *resolved_source = source;
    pas::free(buffer);
    if (error) error->clear();
    return true;
  } catch (...) {
    pas::free(buffer); bytes->clear(); return Fail(error, "cached AFT read failed");
  }
}

std::string ResolveLabelAlias(const std::string& requested, bool smoothing_enabled) {
  const struct Pair { const pas::WideString* plain; const pas::WideString* smooth; } pairs[] = {
      {&GlobalsV::SmallFontName, &GlobalsV::SmoothSmallFontName},
      {&GlobalsV::SmallBoldFontName, &GlobalsV::SmoothSmallBoldFontName},
      {&GlobalsV::NormalFontName, &GlobalsV::SmoothNormalFontName},
      {&GlobalsV::NormalBoldFontName, &GlobalsV::SmoothNormalBoldFontName},
  };
  for (const auto& pair : pairs) {
    const auto plain = Narrow(*pair.plain), smooth = Narrow(*pair.smooth);
    if (smoothing_enabled && requested == plain) return smooth;
    if (!smoothing_enabled && requested == smooth) return plain;
  }
  return requested;
}

}  // namespace srhd_awa::platform::font_repository
