// Original cache + original 3G CPU entry prefix, not a substitute CGame::Init.
#include "init_runtime.hpp"
#include "VectorObjectCpu.hpp"
#include "TextureFlags.hpp"
#include <cassert>
#include <bit>
#include <cstdio>
#include <filesystem>

#ifdef MATRIXGAME_INIT_ALLOC_FAULTS
static unsigned fail_at{},calls{};static bool hit{};
extern "C" void* __real_calloc(std::size_t,std::size_t);
extern "C" void* __wrap_calloc(std::size_t size,std::size_t count) {
    if(fail_at && ++calls==fail_at){fail_at=0;hit=true;return nullptr;}
    return __real_calloc(size,count);
}
#endif
using namespace srhd_awa::matrixgame;
#ifdef MATRIXGAME_INIT_GPU_LINK
extern "C" void MatrixGameCachedMaterialGpuUpload(const MaterialTexture&);
#endif
template<class F> void rejected(F&& f) {
    bool failed=false;try{f();}catch(...){failed=true;}assert(failed);
}
void cold() {
    assert(!g_Cache && !g_CacheHeap && !InspectCpu3GState().bound);
    assert(!CachedTextureBytes() && !CacheTextureEntryCount());
}
int main(int argc,char** argv) {
#ifdef MATRIXGAME_INIT_GPU_LINK
    // Retain the real GPU function at link time, but never call it in the CPU
    // contract or label a missing physical execution as successful.
    auto volatile gpu_path=&MatrixGameCachedMaterialGpuUpload;
    (void)gpu_path;
#endif
    if(argc!=6){std::fprintf(stderr,"usage: init-probe MODEL TEXTURE SHORT_MODEL WIDTH HEIGHT\n");return 2;}
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    try {
        const auto model=std::filesystem::path(argv[1]).wstring();
        const auto texture=std::filesystem::path(argv[2]).wstring();
        const auto truncated=std::filesystem::path(argv[3]).wstring();
        const unsigned width=std::stoul(argv[4]),height=std::stoul(argv[5]);
        // Existing TextureRequest::Load uses original-style CompleteBox/POT
        // preparation. The PNG fixture is 7x5; its retained image is 8x8.
        const auto retained_width=std::bit_ceil(width),retained_height=std::bit_ceil(height);
        int module{},window{};MaterialTexture stale;std::shared_ptr<const TextureAsset> in_flight;
        cold();
        for(unsigned cycle=0;cycle<2;++cycle) {
            NativeInitRuntime runtime;runtime.Begin(&module,&window);
            assert(runtime.CpuReady() && !runtime.GpuReady());
            assert(InspectCpu3GState().module==&module && InspectCpu3GState().window==&window);
            assert(g_Cache && g_CacheHeap && g_Cache->_data.empty());
            auto* cache=g_Cache;auto* heap=g_CacheHeap;
            rejected([&]{runtime.Begin(nullptr,nullptr);});
            rejected([&]{CacheInit();});assert(g_Cache==cache && g_CacheHeap==heap);
            NativeInitRuntime second;rejected([&]{second.Begin(nullptr,nullptr);});
            second.Deinit();assert(runtime.CpuReady() && InspectCpu3GState().module==&module);
            auto* object=g_Cache->Get(CacheClass::VO,model);
            assert(g_Cache->Find(CacheClass::VO,model)==object && object->m_Cache==g_Cache);
            assert(g_Cache->Get(CacheClass::VO,model)==object && g_Cache->_data.size()==1);
            auto* tex=g_Cache->Get(CacheClass::TextureManaged,texture+L"?Alpha");
            auto* plain=g_Cache->Get(CacheClass::Texture,texture);
            assert(tex!=plain && g_Cache->_data.size()==3 && CacheTextureEntryCount()==2);
            g_Cache->PreLoad();assert(object->IsLoaded() && !tex->IsLoaded());
            assert(static_cast<CVectorObject*>(object)->GetFramesCnt()>0);
            auto material=BorrowMaterialTexture(*tex);in_flight=material.Read();
            assert(g_Cache->_data.front()==tex && tex->IsLoaded());
            assert(in_flight->levels[0].width==retained_width && in_flight->levels[0].height==retained_height);
            assert(material.Flags()&TF_ALPHABLEND);
            auto other=BorrowMaterialTexture(*plain);auto plain_data=other.Read();
            assert(plain_data->levels[0].bytes==in_flight->levels[0].bytes);
            const auto bytes=CachedTextureBytes();assert(bytes && plain_data->levels.size()==in_flight->levels.size());
            std::size_t expected{};for(const auto& level:in_flight->levels)expected+=level.bytes.size();
            assert(bytes==expected*2);
            tex->RefInc();assert(tex->RefDecUnload());
            assert(tex->IsLoaded() && CachedTextureBytes()==bytes); // Original managed Unload is a no-op.
            plain->Unload();assert(!plain->IsLoaded() && CachedTextureBytes()==expected);
            assert(other.Read()->levels[0].bytes==in_flight->levels[0].bytes);
            assert(CachedTextureBytes()==bytes);
            rejected([&]{(void)BorrowMaterialTexture(*object);});
            object->RefInc();assert(object->Ref()==1);assert(object->RefDecUnload());assert(!object->IsLoaded());
            object->Prepare();assert(object->IsLoaded() && g_Cache->_data.front()==object);
            auto* missing=g_Cache->Get(CacheClass::TextureManaged,L"missing-material.png");
            auto bad=BorrowMaterialTexture(*missing);rejected([&]{(void)bad.Read();});assert(!missing->IsLoaded());
            auto* short_vo=g_Cache->Get(CacheClass::VO,truncated);rejected([&]{short_vo->Prepare();});
            assert(!short_vo->IsLoaded());
            std::wstring changed;
            CacheReplaceFileExt(changed,L"dir\\\u0442\u0435\u043a\u0441.png?Alpha",L"dds");
            assert(changed==L"dir\\\u0442\u0435\u043a\u0441.dds?Alpha");
            CacheReplaceFileNameAndExt(changed,L"dir\\old.png?Alpha",L"new.dds");
            assert(changed==L"dir\\new.dds?Alpha");
            stale=material;plain_data.reset();runtime.Deinit();runtime.Deinit();
            cold();assert(stale.Expired());rejected([&]{(void)stale.Read();});
            assert(in_flight && in_flight->levels[0].width==retained_width);in_flight.reset();
            std::printf("PASS cycle=%u original CacheInit/Get/Find/Prepare/PreLoad/Up/RefDecUnload/Clear/CacheDeinit; entries=5 decoded_bytes=%zu retained_after_deinit=0\n",cycle,bytes);
        }
        // A foreign CPU context must survive rollback of our later phase.
        L3GInitAsDLLCpuPhase(&module,&window);
        {
            NativeInitRuntime partial;rejected([&]{partial.Begin(nullptr,nullptr);});
            assert(!g_Cache && !g_CacheHeap && InspectCpu3GState().module==&module);
            assert(InspectCpu3GState().bound);partial.Deinit();
        }
        L3GDeinitCpuPhase();cold();
        CacheInit();auto* foreign=g_Cache;
        rejected([&]{foreign->Get(CacheClass::TextureManaged,L"legacy-unbound.png");});
        assert(foreign->_data.empty() && !CacheTextureEntryCount());
        {NativeInitRuntime foreign_cache;rejected([&]{foreign_cache.Begin(nullptr,nullptr);});}
        assert(g_Cache==foreign);CacheDeinit();cold();
#ifdef MATRIXGAME_INIT_ALLOC_FAULTS
        for(unsigned position:{1u,2u}) {
            NativeInitRuntime failure;calls=0;hit=false;fail_at=position;
            rejected([&]{failure.Begin(&module,&window);});fail_at=0;assert(hit);cold();
            failure.Begin(&module,&window);assert(failure.CpuReady());failure.Deinit();cold();
            std::printf("PASS actual CHeap calloc failure=%u rollback and reentry\n",position);
        }
#endif
        std::printf("HOST_INIT_CACHE_PASS; GPU UNSUPPORTED at %s; CGame::Init NOT CALLED\n",NativeInitRuntime::FirstUnsupportedOperation());
    } catch(const Base::CException&) {std::fprintf(stderr,"FAIL MatrixLib exception\n");return 1;}
      catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
