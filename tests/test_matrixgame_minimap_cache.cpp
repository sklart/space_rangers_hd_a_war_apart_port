// SPDX-License-Identifier: GPL-2.0-or-later
// Production original minimap methods + real PNG/CFile. GPU upload/readback
// callbacks below are an explicit CPU driver, never a native Init/frame PASS.
#define MATRIXGAME_SURFACE_DRIVER_ONLY
#include "test_matrixgame_native_surfaces.cpp"
#undef MATRIXGAME_SURFACE_DRIVER_ONLY
#include "Texture.hpp"
class CMatrixMapStatic;
#include "MinimapFixture.hpp"
#include "unmanaged_texture_backend.hpp"
#include "output_paths_backend.hpp"
#include "minimap_lifetime.hpp"
#include <filesystem>
#include <codecvt>
#include <locale>
namespace nt=m::native_texture;namespace ut=m::unmanaged_texture;namespace life=m::minimap_lifetime;namespace op=m::output_paths;
struct NativeMinimapTestAccess {
 static void Seed(CMinimap& minimap,IDirect3DTexture9* texture){assert(!minimap.m_Texture);minimap.m_Texture=m::legacy_texture::CreateUnmanaged();minimap.m_Texture->Set(texture);}
 static IDirect3DTexture9* Backup(CMinimap& minimap){return minimap.m_TextureStore;}
 static bool Save(CMinimap& minimap,const std::wstring& file){return minimap.StoreMinimapToCache(file,minimap.m_Texture->DX());}
 static bool Load(CMinimap& minimap,const std::wstring& file,IDirect3DTexture9* texture){return minimap.LoadMinimapFromCache(file,texture);}
};
struct TexImage final:nt::Image {std::vector<std::byte> bytes;};
struct Imported final:ns::Resource {std::shared_ptr<nt::Texture> texture;};
struct Driver final:Routing,nt::Provider {
 std::shared_ptr<nt::Session> textures=std::make_shared<nt::Session>();unsigned uploads{},fail_upload{};bool fail_import{};
 Driver(){session->textures=textures;textures->upload=[this](const nt::Texture& texture){nt::ValidateUpload(texture);if(++uploads==fail_upload)throw std::bad_alloc();auto image=std::make_shared<TexImage>();image->bytes=texture.levels[0].bytes;return image;};
  session->import_texture=[this](const std::shared_ptr<nt::Texture>& texture,unsigned level){if(fail_import)throw std::logic_error("injected import failure");assert(level==0);auto r=std::make_shared<Imported>();r->texture=texture;(void)nt::Resident(texture);return r;};
  session->read=[this](const std::shared_ptr<ns::Surface>& surface){if(bad_read)throw std::bad_alloc();auto r=std::dynamic_pointer_cast<Imported>(surface->resource);assert(r);return std::dynamic_pointer_cast<TexImage>(nt::Resident(r->texture))->bytes;};
  session->write=[this](const std::shared_ptr<ns::Surface>& surface,std::span<const std::byte> bytes){if(bad_write)throw std::bad_alloc();auto r=std::dynamic_pointer_cast<Imported>(surface->resource);assert(r);std::dynamic_pointer_cast<TexImage>(r->texture->image)->bytes.assign(bytes.begin(),bytes.end());};
 }
 ~Driver(){session->Close();textures->Shutdown();}
 std::shared_ptr<nt::Session> TextureSession()override{return textures;}
};
std::wstring Wide(const char* s){return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(s);}
int main(int argc,char** argv){assert(argc==2);Driver driver;p::Current().ready=true;p::Current().device=&driver;Base::CHeap heap;CCache cache;assert(!g_Cache && !g_CacheHeap);g_Cache=&cache;g_CacheHeap=&heap;auto baseline=m::legacy_texture::RegistrySize();CMinimap minimap;
 auto create=[&](){IDirect3DTexture9* value{};assert(nt::Create(driver.textures,512,512,1,1,22,0,&value)==0);return value;};
 auto payload=[&](IDirect3DTexture9* value)->std::vector<std::byte>&{return std::dynamic_pointer_cast<TexImage>(nt::Resident(nt::Find(value)))->bytes;};
 auto seed=create();auto& pixels=payload(seed);for(unsigned y=0;y<512;++y)for(unsigned x=0;x<512;++x){auto i=(y*512+x)*4;pixels[i]=std::byte(x%251);pixels[i+1]=std::byte(y%241);pixels[i+2]=std::byte((x+y)%239);pixels[i+3]=std::byte{255};}auto expected=pixels;NativeMinimapTestAccess::Seed(minimap,seed);assert(nt::HandleCount()==1);
 driver.fail_import=true;Reject([&]{minimap.StoreTexture();});driver.fail_import=false;assert(minimap.GetTexture() && !NativeMinimapTestAccess::Backup(minimap));assert(nt::HandleCount()==1 && ns::HandleCount()==0);
 minimap.StoreTexture();assert(!minimap.GetTexture() && NativeMinimapTestAccess::Backup(minimap));assert(nt::HandleCount()==1 && !driver.textures->DefaultResourcesLive());
 driver.fail_upload=driver.uploads+2;Reject([&]{minimap.RestoreTexture();});assert(!minimap.GetTexture() && NativeMinimapTestAccess::Backup(minimap));assert(nt::HandleCount()==1 && ns::HandleCount()==0);
 driver.fail_upload=0;minimap.RestoreTexture();assert(minimap.GetTexture() && !NativeMinimapTestAccess::Backup(minimap));assert(payload(minimap.GetTexture()->DX())==expected && nt::HandleCount()==1);
 // Actual original PNG encode/decode and system/default transfers, with CPU GPU driver.
 const auto root=Wide(argv[1]);op::NativeScope outputs(root.c_str());assert(op::CreateFolder(root.c_str()));auto file=root+L"/Cache/minimap.png";assert(NativeMinimapTestAccess::Save(minimap,file));assert(payload(minimap.GetTexture()->DX())==expected && nt::HandleCount()==1 && ns::HandleCount()==0);
 {life::TextureOwner target;auto result=create();*target.put()=result;assert(NativeMinimapTestAccess::Load(minimap,file,target));target.detach();assert(payload(minimap.GetTexture()->DX())==expected);}
 {life::TextureOwner target;*target.put()=create();driver.fail_upload=driver.uploads+1;Reject([&]{NativeMinimapTestAccess::Load(minimap,file,target);});driver.fail_upload=0;assert(payload(minimap.GetTexture()->DX())==expected && nt::HandleCount()==2);}
 assert(nt::HandleCount()==1 && ns::HandleCount()==0);
 driver.bad_read=true;Reject([&]{NativeMinimapTestAccess::Save(minimap,root+L"/Cache/read-failure.png");});driver.bad_read=false;assert(nt::HandleCount()==1 && ns::HandleCount()==0 && payload(minimap.GetTexture()->DX())==expected);
 // The exact production lock scope unlocks while another owner retains a system surface.
 {life::TextureOwner system;assert(nt::Create(driver.textures,16,16,1,0,22,2,system.put())==0);os::SurfaceOwner surface;ut::Surface(system,0,surface.put());auto retained=ns::Find(surface);try{m::legacy_texture::LockedRect rect{};os::SurfaceLockScope lock{surface,rect};static_cast<std::byte*>(rect.pBits)[0]=std::byte{42};throw std::bad_alloc();}catch(const std::bad_alloc&){}assert(!retained->locked && !nt::Find(system)->levels[0].locked && nt::Find(system)->levels[0].bytes[0]==std::byte{42});}
 {unsigned flags=5;try{life::FlagRollback rollback{flags,4};flags|=8;flags&=~4u;throw std::bad_alloc();}catch(const std::bad_alloc&){}assert(flags==13);{life::FlagRollback rollback{flags,4};flags&=~4u;rollback.Commit();}assert(flags==9);bool value=true;try{life::ValueRollback rollback{value};value=false;throw std::bad_alloc();}catch(const std::bad_alloc&){}assert(value);}
 minimap.StoreTexture();driver.fail_upload=driver.uploads+1;minimap.RestoreTexture();driver.fail_upload=0;assert(!minimap.GetTexture() && !NativeMinimapTestAccess::Backup(minimap)); // Preserve original HRESULT allocation-failure discard branch.
 auto another=create();NativeMinimapTestAccess::Seed(minimap,another);driver.bad_read=true;minimap.StoreTexture();driver.bad_read=false;assert(!minimap.GetTexture() && !NativeMinimapTestAccess::Backup(minimap));
 minimap.Clear();assert(nt::HandleCount()==0 && ns::HandleCount()==0 && m::legacy_texture::RegistrySize()==baseline);g_Cache=nullptr;g_CacheHeap=nullptr;p::Current().ready=false;p::Current().device=nullptr;
 std::puts("original minimap StoreTexture/RestoreTexture/PNG save-load/adoption/transfer-OOM/import-failure/lock-scope/flag-unwind/no-leaks PASS CPU_GPU_DRIVER_MODEL_ONLY NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN");
}
