#include "renderer_platform.hpp"
#include "runtime_settings_slice.hpp"
#include "startup_slice.hpp"
#include "ui_metadata_slice.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Cache.hpp"
#include "units/EC_CacheBitmap.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_File.hpp"
#include "units/GR_Main.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <zlib.h>

namespace {
struct Candidate { pas::WideString key; std::string text; std::string file; int priority{}; };
struct Result { std::int32_t source_size{}, width{}, height{}, bpp{}, pitch{}, resident{}; std::uint32_t crc{}; std::uint64_t fnv{}; };

std::string Narrow(const pas::WideString& value) { return static_cast<pas::AnsiString>(value).c_str(); }
std::string Lower(std::string value) { for (char& c : value) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return value; }
int Priority(const std::string& key) { const auto low=Lower(key); if(low.find("font")!=std::string::npos) return 0; if(low.find("ui")!=std::string::npos || low.find("panel")!=std::string::npos) return 1; return 2; }
void Collect(EC_Data::TDataEC* root, const pas::WideString& prefix, std::vector<Candidate>* out) {
  if (!root) return;
  for (auto* entry=root->FirstEntry;entry;entry=entry->Next) {
    const pas::WideString path=prefix.length()==0?entry->Name:pas::concat_wide({prefix,u"."_wref.get(),entry->Name});
    if (entry->Kind==EC_Data::dekFile) {
      Candidate candidate{}; candidate.key=path; candidate.text=Narrow(path); candidate.priority=Priority(candidate.text);
      if(entry->SharedFileRef && entry->SharedFileRef->FileRef) candidate.file=Narrow(entry->SharedFileRef->FileRef->FileName);
      out->push_back(std::move(candidate));
    } else if(entry->Kind==EC_Data::dekSubtree) Collect(entry->ChildData,path,out);
  }
}
std::uint64_t Fnv1a(const void* pixels, std::size_t count) { const auto* p=static_cast<const std::uint8_t*>(pixels);std::uint64_t result=UINT64_C(1469598103934665603);for(std::size_t i=0;i<count;++i){result^=p[i];result*=UINT64_C(1099511628211);}return result; }
bool Decode(EC_Cache::TCacheEC* cache,const Candidate& candidate,Result* result) {
  EC_Buf::TBufEC* source=nullptr;auto* bitmap=pas::construct_call<EC_CacheBitmap::TCBitmapEC>(EC_CacheBitmap::TCBitmapEC_Create);bool ok=false;
  try { source=cache->OpenDataBuffer(candidate.key);result->source_size=source?source->DataSize:-1;bitmap->LoadFromConfigBuffer(source,pas::WideString());auto* graph=bitmap->Bitmap;const auto bytes=static_cast<std::size_t>(graph->PitchBytes)*graph->Height;result->width=graph->Width;result->height=graph->Height;result->bpp=graph->BytesPerPixel;result->pitch=graph->PitchBytes;result->resident=bitmap->ResidentBytes;result->crc=crc32(0,static_cast<const Bytef*>(graph->GetPixels()),static_cast<uInt>(bytes));result->fnv=Fnv1a(graph->GetPixels(),bytes);ok=graph->Width>0&&graph->Height>0&&graph->GetPixels()!=nullptr&&bitmap->ResidentBytes==graph->PitchBytes*graph->Height;}catch(...){ok=false;}
  pas::free(source);pas::free(bitmap);return ok;
}
bool Same(const Result& a,const Result& b) { return a.width==b.width&&a.height==b.height&&a.bpp==b.bpp&&a.pitch==b.pitch&&a.resident==b.resident&&a.crc==b.crc&&a.fnv==b.fnv; }
}

int main(int argc,char** argv) {
  if(argc!=3) { std::fputs("usage: test_bitmap_cache_release <game-root> <user-root>\n",stderr);return 2; }
  const std::filesystem::path game_root(argv[1]),user_root(argv[2]);std::error_code fs_error;std::filesystem::remove_all(user_root,fs_error);
  srhd_awa::platform::startup_slice::State startup;std::string error;bool ok=srhd_awa::platform::startup_slice::Initialize(&startup,game_root.string(),user_root.string(),(user_root/"gr-main.log").string(),&error);
  if(!ok){std::fprintf(stderr,"startup failed: %s\n",error.c_str());return 1;}
  ok=srhd_awa::platform::runtime_settings_slice::Initialize(&error);if(!ok){std::fprintf(stderr,"runtime failed: %s\n",error.c_str());srhd_awa::platform::startup_slice::Shutdown(&startup);return 1;}
  ok=srhd_awa::platform::ui_metadata_slice::Initialize(&error);if(!ok){std::fprintf(stderr,"metadata failed: %s\n",error.c_str());srhd_awa::platform::runtime_settings_slice::Shutdown();srhd_awa::platform::startup_slice::Shutdown(&startup);return 1;}
  for(const auto& font:srhd_awa::platform::ui_metadata_slice::FontResolutions())std::printf("font key=%s found=%u kind=%u file=%s exists=%u\n",font.key.c_str(),font.found?1u:0u,font.kind,font.filename.c_str(),font.file_exists?1u:0u);
  ok=srhd_awa::platform::renderer_platform::InitializeReleaseCompatibleDefaults(&error);if(!ok){std::fprintf(stderr,"renderer failed: %s\n",error.c_str());}
  std::vector<Candidate> candidates;Collect(GR_Main::CacheDataRoot,pas::WideString(),&candidates);std::sort(candidates.begin(),candidates.end(),[](const Candidate&a,const Candidate&b){return a.priority!=b.priority?a.priority<b.priority:a.text<b.text;});
  Candidate chosen{};Result first{},second{};for(const auto& candidate:candidates){if(!GR_Main::CacheDataRoot->FileExistsByPath(candidate.key))continue;if(Decode(GR_Main::GlobalCache,candidate,&first)&&Decode(GR_Main::GlobalCache,candidate,&second)){chosen=candidate;break;}}
  const bool repeated=!chosen.text.empty()&&Same(first,second);ok=ok&&repeated&&GR_Main::GlobalCache&&GR_Main::CacheDataRoot;
  if(!chosen.text.empty())std::printf("release bitmap key=%s file=%s source_size=%d decoded=%dx%d bpp=%d pitch=%d resident=%d crc32=%08x fnv64=%016llx repeat=%u\n",chosen.text.c_str(),chosen.file.c_str(),first.source_size,first.width,first.height,first.bpp,first.pitch,first.resident,first.crc,static_cast<unsigned long long>(first.fnv),repeated?1u:0u);else std::fputs("no decodable release bitmap found\n",stderr);
  srhd_awa::platform::renderer_platform::ShutdownSoftwareRenderer();auto* cache=GR_Main::GlobalCache;auto* root=GR_Main::CacheDataRoot;srhd_awa::platform::ui_metadata_slice::Shutdown();ok=ok&&GR_Main::GlobalCache==cache&&GR_Main::CacheDataRoot==root;srhd_awa::platform::runtime_settings_slice::Shutdown();srhd_awa::platform::startup_slice::Shutdown(&startup);std::filesystem::remove_all(user_root,fs_error);
  if (ok) std::puts("M13 release bitmap probe PASS");
  return ok ? 0 : 1;
}