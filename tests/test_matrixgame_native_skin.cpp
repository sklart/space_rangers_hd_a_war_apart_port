// Original skin resource selection/Takt in a real cache -> VO -> frame path.
#include "native_skin.hpp"
#include "CStorage.hpp"
#include "TextureFlags.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
using namespace srhd_awa::matrixgame;
#ifdef MATRIXGAME_SESSION_GPU_LINK
extern "C" void MatrixGameNativeSkinDraw(const wchar_t*);
#endif
#ifdef MATRIXGAME_SKIN_ALLOC_FAULTS
static unsigned fail_new{},new_calls{};static bool allocation_hit{};
extern "C" void* __real__Znwm(std::size_t);
extern "C" void* __wrap__Znwm(std::size_t n) {
    if(fail_new && ++new_calls==fail_new){fail_new=0;allocation_hit=true;throw std::bad_alloc();}
    return __real__Znwm(n);
}
#endif
template<class F> void rejected(F&& f) {bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(int argc,char** argv) {
#ifdef MATRIXGAME_SESSION_GPU_LINK
    auto volatile keep=&MatrixGameNativeSkinDraw;(void)keep;
#endif
    if(argc!=4)return 2;
    try {
        const auto model=std::filesystem::path(argv[1]).wstring();
        const auto texture=std::filesystem::path(argv[2]).wstring();
        // Controlled derived VO changes only surfs/texs to point at the PNG
        // fixture. Original source and its read-only copy are never written.
        {
            CStorage storage;assert(storage.Load(model.c_str()));
            auto* names=storage.GetBuf(L"surfs",L"texs",ST_WCHAR);assert(names && names->GetArraysCount());
            for(unsigned i=0;i<names->GetArraysCount();++i)names->SetWStr(i,texture);
            storage.Save(L"controlled-skin.vo");
        }
        std::shared_ptr<const NativeSkin> stale;
        for(int cycle=0;cycle<2;++cycle) {
            NativeInitRuntime runtime;runtime.Begin(nullptr,nullptr);
            NativeSkinLibrary skins(runtime,true);
            const auto skin=skins.Acquire(texture.c_str(),GSP_ORDINAL);
            assert(skin->Valid() && skin->group==GSP_ORDINAL && skins.Size()==1);
            assert(skins.Acquire(texture.c_str(),GSP_ORDINAL)==skin && skins.Size()==1);
            const auto side=skins.Acquire(texture.c_str(),GSP_SIDE);
            assert(side->group==GSP_SIDE_NOALPHA);
            const auto alpha=skins.Acquire((texture+L"?Alpha").c_str(),GSP_SIDE);
            assert(alpha->group==GSP_SIDE);
            const auto auto_gloss=skins.Acquire(L"base",GSP_SIDE);
            assert(auto_gloss->m_TexGloss);auto_gloss->Preload();
            const auto no_gloss=skins.Acquire(L"base*.",GSP_SIDE);
            assert(!no_gloss->m_TexGloss);
            {NativeSkinLibrary disabled(runtime,false);
                assert(!disabled.Acquire(L"base",GSP_SIDE)->m_TexGloss);}
            rejected([&]{NativeSinglePass(*alpha,1,{});});
            const auto moving_key=texture+L"*.*"+texture+L"*"+texture+L"*0.001,-0.002";
            const auto moving=skins.Acquire(moving_key.c_str(),GSP_ORDINAL);
            assert(!moving->m_TexGloss && moving->m_TexMask && moving->m_TexBack);
            moving->Preload();
            skins.Takt(750);
            assert(std::fabs(moving->m_tu-.75f)<.0001f && std::fabs(moving->m_tv+.5f)<.0001f);
            skins.Takt(750);
            assert(std::fabs(moving->m_tu-.5f)<.0001f && std::fabs(moving->m_tv+1.f)<.0001f);
            assert(skins.Acquire(moving_key.c_str(),GSP_ORDINAL)==moving);
            rejected([&]{NativeSinglePass(*moving,1,{});});
            const auto gloss=skins.Acquire((texture+L"*"+texture).c_str(),GSP_ORDINAL);
            assert(gloss->m_TexGloss);rejected([&]{NativeSinglePass(*gloss,1,{});});
            const auto tested=skins.Acquire((texture+L"?Trans").c_str(),GSP_ORDINAL);
            assert(NativeSinglePass(*tested,1,{}).state.raster.alpha_test);
            const auto count=skins.Size();
            rejected([&]{skins.Acquire(nullptr,GSP_ORDINAL);});
            rejected([&]{skins.Acquire(L"",GSP_ORDINAL);});
            rejected([&]{skins.Acquire(texture.c_str(),GSP_COUNT);});
            rejected([&]{skins.Acquire(L"missing-native-skin.png",GSP_ORDINAL);});
            rejected([&]{skins.Acquire(L"bad-material.png",GSP_ORDINAL);});
            assert(skins.Size()==count && skin->Valid());
            auto* object=static_cast<CVectorObject*>(g_Cache->Get(CacheClass::VO,L"controlled-skin.vo"));
            const auto materials=skins.SurfaceMaterials(*object,{});
            const auto frame=PrepareCachedFrame(runtime,*object,0,materials);
            assert(materials.size()==std::size_t(object->NativeSurfaceCount()) && !frame.submissions.empty());
            stale=skin;skins.Clear();skins.Clear();assert(!stale->Valid() && !skins.Size());
            rejected([&]{stale->Preload();});
            assert(skins.Acquire(texture.c_str(),GSP_ORDINAL)->Valid());
#ifdef MATRIXGAME_SKIN_ALLOC_FAULTS
            for(unsigned position:{1u,3u,6u}) {
                skins.Clear();new_calls=0;allocation_hit=false;fail_new=position;
                rejected([&]{skins.Acquire(texture.c_str(),GSP_ORDINAL);});
                fail_new=0;assert(allocation_hit && !skins.Size());
                assert(skins.Acquire(texture.c_str(),GSP_ORDINAL)->Valid());
                std::printf("PASS actual C++ allocation failure=%u; resource publication rollback/retry\n",position);
            }
#endif
            auto dead=skins.Acquire(texture.c_str(),GSP_ORDINAL);
            runtime.Deinit();assert(!dead->Valid());
            rejected([&]{skins.Takt(1);});skins.Clear();
            assert(!g_Cache && !g_CacheHeap && !CachedTextureBytes());
            std::printf("PASS cycle=%d original skin parsing/dedup/Takt -> VO surfaces -> cached frame; cleanup/reentry\n",cycle);
        }
        std::puts("SKIN_RESOURCE_RUNTIME_PASS; original GPU Prepare/callbacks UNSUPPORTED; CGame::Init NOT CALLED");
    }catch(const Base::CException&){std::fputs("FAIL MatrixLib exception\n",stderr);return 1;}
     catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
