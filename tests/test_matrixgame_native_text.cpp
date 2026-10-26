// Actual original Text::Render and its production native backend. Low-level
// image upload is a CPU fault model; it is NOT deko3d execution.
#include "native_text.hpp"
#include "native_texture.hpp"
#include "Text/Render.hpp"
#include "Text/Font.hpp"
#include <fstream>
#include <cassert>
#include <cstdio>
#include <cstring>
namespace p=srhd_awa::matrixgame::platform3g;
namespace t=srhd_awa::matrixgame::native_texture;
namespace text=srhd_awa::matrixgame::text_backend;
namespace n=text::native;
std::vector<std::byte> assets[4];
std::vector<std::byte> Read(const std::string& path){std::ifstream f(path,std::ios::binary);assert(f);std::vector<char> raw((std::istreambuf_iterator<char>(f)),{});std::vector<std::byte> out(raw.size());std::memcpy(out.data(),raw.data(),raw.size());return out;}

struct Image final:t::Image{};
struct Routing final:p::Device,t::Provider,n::Provider{
 std::shared_ptr<t::Session> session=std::make_shared<t::Session>();unsigned uploads{},failAt{};bool fail{};
 Routing(){session->upload=[this](const t::Texture& value){t::ValidateUpload(value);if(fail || uploads+1==failAt)throw std::bad_alloc();++uploads;return std::make_shared<Image>();};}
 ~Routing(){session->Shutdown();}
 std::span<const std::byte> TextFontAsset(unsigned id)override{return assets[id];}
 std::shared_ptr<t::Session> TextureSession()override{return session;}
 p::DeviceCaps ReadCaps()override{throw p::Unsupported("text test has no GPU caps");}
 std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("text test has no GPU budget");}
 p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("text test has no VI gamma");}
 void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
};
int main(int argc,char** argv){assert(argc==2);unsigned total=0;std::vector<std::byte> menu;
 for(unsigned id=0;id<4;++id){assets[id]=Read(std::string(argv[1])+"/font"+std::to_string(id)+".bin");auto data=n::Data::Load(assets[id]);auto bytes=Read(std::string(argv[1])+"/font"+std::to_string(id)+".golden");std::size_t at=0;
 auto word=[&](){assert(at+4<=bytes.size());unsigned v=0;for(unsigned k=0;k<4;++k)v|=unsigned(bytes[at++])<<(8*k);return v;};
 while(at<bytes.size()){unsigned count=word();assert(count<65536);std::wstring s;for(unsigned k=0;k<count;++k)s.push_back(wchar_t(word()));auto flags=word();auto color=word();int result=int(word());text::Rect expected{int(word()),int(word()),int(word()),int(word())};text::Rect rect{2,-2,73,20};std::vector<std::byte> pixels(75*20*4);auto actual=n::Draw(*data,s.data(),int(s.size()),rect,flags,color,pixels,75,20,75*4);assert(actual==result && std::memcmp(&rect,&expected,sizeof(rect))==0);assert(at+pixels.size()<=bytes.size() && std::memcmp(pixels.data(),bytes.data()+at,pixels.size())==0);if(id==0 && s==L"Меню" && flags==0x115 && color==0xffb0ab99)menu=pixels;at+=pixels.size();++total;}
 if(id){text::Rect bounds{};bool shaping=false;try{n::Draw(*data,L"A\u0300",2,bounds,0x520,0,{},0,0,0);}catch(const p::Unsupported&){shaping=true;}assert(shaping);}
 auto bad=assets[id];bad.resize(33);bool rejected=false;try{n::Data::Load(bad);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
  if(id==0){bool missing=false;try{data->Lookup(wchar_t(0x451));}catch(const p::Unsupported&){missing=true;}assert(missing);} // Original Rangers has no yo; no substitute font.
 }
 Routing routing;p::Current().ready=true;p::Current().device=&routing;auto identity=text::DeviceIdentity();assert(identity==text::DeviceIdentity());
 for(unsigned cycle=0;cycle<2;++cycle){CBitmap bitmap;Text::Render(L"Меню",L"Font.2Ranger",0xffb0ab99,75,20,1,1,0,0,-2,Base::CRect(0,0,75,20),bitmap);assert(bitmap.Data() && bitmap.SizeX()==75 && bitmap.SizeY()==20);assert(std::memcmp(bitmap.Data(),menu.data(),menu.size())==0);assert(t::HandleCount()==0);Text::ClearFontCache();}
 // D3DX factory outputs are write-only. A caller need not initialize the slot.
 // Poisoned values make accidental input validation deterministic under ASan;
 // they are never registered objects and must never be released or dereferenced.
 {
  auto poisonFont=reinterpret_cast<ID3DXFont*>(std::uintptr_t(1));
  assert(text::CreateTextFont(identity,L"Verdana",12,&poisonFont)==0);
  text::FontDesc desc{};assert(text::DescribeFont(poisonFont,&desc)==0 && desc.Height==12);
  text::Release(poisonFont);
  poisonFont=reinterpret_cast<ID3DXFont*>(std::uintptr_t(1));bool rejected=false;
  try{text::CreateTextFont(nullptr,L"Verdana",12,&poisonFont);}catch(const std::invalid_argument&){rejected=true;}
  assert(rejected && !poisonFont);
  poisonFont=reinterpret_cast<ID3DXFont*>(std::uintptr_t(1));rejected=false;
  try{text::CreateTextFont(identity,L"Unsupported",12,&poisonFont);}catch(const p::Unsupported&){rejected=true;}
  assert(rejected && !poisonFont);
  IDirect3DTexture9* texture{};assert(text::CreateTexture(75,20,&texture)==0);
  auto poisonSurface=reinterpret_cast<IDirect3DSurface9*>(std::uintptr_t(1));
  assert(text::GetSurface(texture,0,&poisonSurface)==0);text::Release(poisonSurface);
  poisonSurface=reinterpret_cast<IDirect3DSurface9*>(std::uintptr_t(1));rejected=false;
  try{text::GetSurface(texture,~0u,&poisonSurface);}catch(const std::out_of_range&){rejected=true;}
  assert(rejected && !poisonSurface);text::Release(texture);
  auto poisonTarget=reinterpret_cast<ID3DXRenderToSurface*>(std::uintptr_t(1));
  assert(text::CreateRenderSurface(75,20,&poisonTarget)==0);text::Release(poisonTarget);
  poisonTarget=reinterpret_cast<ID3DXRenderToSurface*>(std::uintptr_t(1));rejected=false;
  try{text::CreateRenderSurface(0,20,&poisonTarget);}catch(const std::invalid_argument&){rejected=true;}
  assert(rejected && !poisonTarget && t::HandleCount()==0);
 }
 // Exception during the real EndScene upload must drop renderer/surface/texture,
 // preserve font cache for retry, and never leave an active scene or lock.
 routing.failAt=routing.uploads+2;bool failed=false;try{CBitmap b;Text::Render(L"Меню",L"Font.2Ranger",~0u,75,20,0,0,0,0,0,Base::CRect(0,0,75,20),b);}catch(const std::bad_alloc&){failed=true;}assert(failed && t::HandleCount()==0);routing.failAt=0;routing.fail=false;
 {CBitmap b;Text::Render(L"<color=255,0,0>Меню</color>",L"Font.2Ranger",~0u,75,20,0,0,0,0,0,Base::CRect(0,0,75,20),b);assert(b.Data());} // Original parser executes.
 // Surface owns its texture after the original texture handle is released.
 IDirect3DTexture9* texture{};IDirect3DSurface9* surface{};ID3DXRenderToSurface* renderer{};
 assert(text::CreateTexture(75,20,&texture)==0);auto owner=t::Find(texture);assert(text::GetSurface(texture,0,&surface)==0);assert(text::CreateRenderSurface(75,20,&renderer)==0);text::Release(texture);texture=nullptr;assert(text::BeginScene(renderer,surface)==0);text::Release(surface);assert(text::Clear()==0);
 auto& font=Text::GetFont(identity,L"Font.2Ranger");text::Rect r{2,-2,73,20};text::DrawFontText(font.operator->(),L"Меню",4,&r,0x115,0xffb0ab99);auto old=owner->levels[0].bytes;auto image=owner->image;routing.fail=true;failed=false;try{text::EndScene(renderer,0);}catch(const std::bad_alloc&){failed=true;}assert(failed && owner->levels[0].bytes==old && owner->image==image);routing.fail=false;assert(text::EndScene(renderer,0)==0);assert(owner->levels[0].bytes==menu);text::Release(renderer);owner.reset();
 // Managed text textures retain CPU data through GPU resource invalidation.
 assert(text::CreateTexture(75,20,&texture)==0);owner=t::Find(texture);routing.session->Invalidate();assert(!owner->image);t::Preload(texture);assert(owner->image);text::Release(texture);texture=nullptr;owner.reset();
 Routing next;p::Current().device=&next;assert(text::DeviceIdentity()!=identity);Text::GetFont(text::DeviceIdentity(),L"Font.2Ranger");Text::ClearFontCache();assert(t::HandleCount()==0);
 p::Current().device=nullptr;p::Current().ready=false;
 std::printf("original Text::Render Top/mm exact BGRA PASS; glyph/layout goldens=%u; ownership/abort/upload-failure/retry/cache-rebind/managed-reupload/write-only-factory-outputs PASS; CPU_UPLOAD_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN\n",total);
}
