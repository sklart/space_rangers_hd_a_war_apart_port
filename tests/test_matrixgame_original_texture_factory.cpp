// Original cache classes must never alias the distinct CPU material cache type.
#include "init_runtime.hpp"
#include "cache_resources.hpp"
#include "Texture.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame;
namespace {
CCacheData* Original(CacheClass type){
 switch(type){
 case CacheClass::Texture:return HNew(g_CacheHeap) CTexture;
 case CacheClass::TextureManaged:return HNew(g_CacheHeap) CTextureManaged;
 default:throw std::invalid_argument("unexpected original texture class");
 }
}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
void CpuMaterial(){
 auto* data=g_Cache->Create(CacheClass::TextureManaged);assert(data && !dynamic_cast<CBaseTexture*>(data));
 assert(!BorrowMaterialTexture(*data).Expired());g_Cache->Destroy(data);assert(!CacheTextureEntryCount());
}
}
int main(){
 Reject([]{OriginalTextureFactoryScope unbound(Original);});
 for(int cycle=0;cycle<2;++cycle){
  NativeInitRuntime runtime;runtime.Begin(nullptr,nullptr);CpuMaterial();
  Reject([]{OriginalTextureFactoryScope invalid(nullptr);});
  {
   OriginalTextureFactoryScope scope(Original);Reject([]{OriginalTextureFactoryScope duplicate(Original);});
   auto* managed=g_Cache->Create(CacheClass::TextureManaged);auto* device=g_Cache->Create(CacheClass::Texture);
   assert(dynamic_cast<CTextureManaged*>(managed) && dynamic_cast<CTexture*>(device));
   assert(managed->m_Type==CacheClass::TextureManaged && device->m_Type==CacheClass::Texture);
   assert(!CacheTextureEntryCount());Reject([&]{BorrowMaterialTexture(*managed);});
   g_Cache->Destroy(managed);g_Cache->Destroy(device);
  }
  CpuMaterial();runtime.Deinit();assert(!g_Cache && !g_CacheHeap && !g_MatrixHeap && !NativeCacheFactoryBound());
 }
 std::puts("ORIGINAL_TEXTURE_FACTORY actual_dynamic_types=PASS cpu_material_preserved=PASS exclusive_scope=PASS cleanup_reentry=PASS NO_GPU");
}
