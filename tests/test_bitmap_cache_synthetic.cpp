#include "ec_file_adapter.hpp"
#include "renderer_platform.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Cache.hpp"
#include "units/EC_CacheBitmap.hpp"
#include "units/EC_Data.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
void Put32(Bytes* b, std::size_t o, std::uint32_t v) { for (std::size_t i=0;i!=4;++i) (*b)[o+i]=static_cast<std::uint8_t>(v>>(8*i)); }
Bytes FixtureBmp() {
  Bytes b(70,0); b[0]='B'; b[1]='M'; Put32(&b,2,70); Put32(&b,10,54); Put32(&b,14,40); Put32(&b,18,2); Put32(&b,22,2); b[26]=1; b[28]=24; Put32(&b,34,16);
  const std::array<std::uint8_t,16> pixels={255,0,0,255,255,255,0,0, 0,0,255,0,255,0,0,0}; std::memcpy(b.data()+54,pixels.data(),pixels.size()); return b;
}
std::uint64_t Fnv1a(const void* data,std::size_t size) { const auto* b=static_cast<const std::uint8_t*>(data); std::uint64_t r=UINT64_C(1469598103934665603); for(std::size_t i=0;i<size;++i){r^=b[i];r*=UINT64_C(1099511628211);} return r; }
EC_Buf::TBufEC* MakeBuffer(const Bytes& b) { auto* r=pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create); r->SetSize(static_cast<std::int32_t>(b.size())); if(!b.empty())std::memcpy(r->Data,b.data(),b.size()); return r; }
struct Result { std::int32_t source_size{},w{},h{},pitch{},bpp{},resident{}; std::uint64_t hash{}; };
bool Decode(EC_Cache::TCacheEC* cache,const pas::WideString& option,Result* r) {
  EC_Buf::TBufEC* source=nullptr; auto* bitmap=pas::construct_call<EC_CacheBitmap::TCBitmapEC>(EC_CacheBitmap::TCBitmapEC_Create); bool ok=false;
  try { source=cache->OpenDataBuffer(u"Bitmap"_wref.get()); r->source_size=source->DataSize; bitmap->LoadFromConfigBuffer(source,option); auto* g=bitmap->Bitmap; r->w=g->Width;r->h=g->Height;r->pitch=g->PitchBytes;r->bpp=g->BytesPerPixel;r->resident=bitmap->ResidentBytes;r->hash=Fnv1a(g->GetPixels(),static_cast<std::size_t>(g->PitchBytes)*g->Height);ok=true;}catch(const pas::Raised& error){std::fprintf(stderr,"bitmap decode error=%s\n",error.what());}catch(...){std::fputs("bitmap decode unknown error\n",stderr);}
  pas::free(source);pas::free(bitmap);return ok;
}
bool Rejects(const Bytes& b) { auto* source=MakeBuffer(b);auto* bitmap=pas::construct_call<EC_CacheBitmap::TCBitmapEC>(EC_CacheBitmap::TCBitmapEC_Create);bool rejected=false;try{bitmap->LoadFromConfigBuffer(source,pas::WideString());}catch(...){rejected=true;}const bool clean=bitmap->Bitmap&&bitmap->Bitmap->Width==0&&bitmap->Bitmap->Height==0&&bitmap->Bitmap->Pixels==nullptr&&bitmap->ResidentBytes==0;pas::free(source);pas::free(bitmap);return rejected&&clean; }
bool AddFileEntry(EC_Data::TDataEC* root) { auto* e=root->AddEntry(EC_Data::dekFile);e->Name=u"Bitmap"_w;e->SharedFileRef=root->InternFileName(u"image.bmp"_w);e->FileOffset=0;e->ByteCount=-1;root->RebuildIndex();return root->FileExistsByPath(u"Bitmap"_wref.get())!=0; }
}
int main() {
  const std::filesystem::path root="build/m13-bitmap";std::error_code error;std::filesystem::remove_all(root,error);std::filesystem::create_directories(root);const auto fixture=FixtureBmp();{std::ofstream f(root/"image.bmp",std::ios::binary);f.write(reinterpret_cast<const char*>(fixture.data()),fixture.size());}
  srhd_awa::platform::ec_file::SetGameRoot(root.string());if(!aPacket::InitializePackageCollection())return 1;GR_Main::CCInterface=pas::construct_call<GR_Main::TCCInterface>(GR_Main::TCCInterface_Create);GR_Main::CacheDataRoot=pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create);GR_Main::GlobalCache=pas::construct_call<EC_Cache::TCacheEC>(EC_Cache::TCacheEC_Create);bool ok=AddFileEntry(GR_Main::CacheDataRoot);GR_Main::GlobalCache->SetDataRoot(GR_Main::CacheDataRoot);
  srhd_awa::platform::renderer_platform::RendererConfig config{};config.game_width=config.game_height=config.presentation_width=config.presentation_height=64;config.minimap_buffer_size=16;std::string renderer_error;ok=ok&&srhd_awa::platform::renderer_platform::InitializeSoftwareRenderer(config,&renderer_error);
  const struct {const char16_t* option;std::int32_t bpp;std::int32_t pitch;} options[]={{u"",2,4},{u"RGBA",4,8},{u"RGB",3,8},{u"Gray",1,4}};
  for(const auto& o:options){Result a{},b{};const bool decoded=Decode(GR_Main::GlobalCache,pas::WideString(o.option),&a)&&Decode(GR_Main::GlobalCache,pas::WideString(o.option),&b);const bool stable=decoded&&a.w==2&&a.h==2&&a.bpp==o.bpp&&a.pitch==o.pitch&&a.resident==o.pitch*2&&std::memcmp(&a,&b,sizeof(Result))==0;if(!stable)std::fprintf(stderr,"bitmap option failed decoded=%d source=%d w=%d h=%d bpp=%d pitch=%d resident=%d hash=%llu\n",decoded,a.source_size,a.w,a.h,a.bpp,a.pitch,a.resident,static_cast<unsigned long long>(a.hash));ok=ok&&stable;}
  Bytes truncated=fixture;truncated.resize(20);Bytes invalid(54,0x5a);Bytes oversized=fixture;Put32(&oversized,18,0x7fffffff);Put32(&oversized,22,0x7fffffff);const bool empty_rejected=Rejects({});const bool truncated_rejected=Rejects(truncated);const bool invalid_rejected=Rejects(invalid);const bool oversized_rejected=Rejects(oversized);if(!(empty_rejected&&truncated_rejected&&invalid_rejected&&oversized_rejected))std::fprintf(stderr,"bitmap corrupt failed empty=%d truncated=%d invalid=%d oversized=%d\\n",empty_rejected,truncated_rejected,invalid_rejected,oversized_rejected);ok=ok&&empty_rejected&&truncated_rejected&&invalid_rejected&&oversized_rejected;
  srhd_awa::platform::renderer_platform::ShutdownSoftwareRenderer();pas::free(GR_Main::GlobalCache);GR_Main::GlobalCache=nullptr;pas::free(GR_Main::CacheDataRoot);GR_Main::CacheDataRoot=nullptr;pas::free(GR_Main::CCInterface);GR_Main::CCInterface=nullptr;aPacket::FinalizePackageCollection();std::filesystem::remove_all(root,error);if(ok)std::puts("M13 bitmap cache synthetic regression PASS");return ok?0:1;
}