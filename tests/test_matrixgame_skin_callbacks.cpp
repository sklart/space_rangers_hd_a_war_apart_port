// Original LoadSpecial/BeforeDraw/Draw with actual SSkin callback dispatch.
#include "skin_pipeline.hpp"
#include "CStorage.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
using namespace srhd_awa::matrixgame;
#ifdef MATRIXGAME_CALLBACK_ALLOC_FAULTS
static bool fail_new{},allocation_hit{};
extern "C" void* __real__Znwm(std::size_t);
extern "C" void* __wrap__Znwm(std::size_t n) {
    if(fail_new){fail_new=false;allocation_hit=true;throw std::bad_alloc();}
    return __real__Znwm(n);
}
struct AllocationFailureProvider:SkinProvider {
    NativeSkinPipeline& pipeline;int calls{};
    explicit AllocationFailureProvider(NativeSkinPipeline& p):pipeline(p){}
    std::shared_ptr<const SSkin> AcquireSkin(const wchar_t* name,std::uint32_t group) override {
        if(++calls==2)fail_new=true;
        return pipeline.AcquireSkin(name,group);
    }
};
#endif
#ifdef MATRIXGAME_SESSION_GPU_LINK
extern "C" void MatrixGameNativeCallbackDraw(const wchar_t*);
#endif
template<class F> void rejected(F&& f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(int argc,char** argv) {
#ifdef MATRIXGAME_SESSION_GPU_LINK
    auto volatile keep=&MatrixGameNativeCallbackDraw;(void)keep;
#endif
    if(argc!=4)return 2;
    try {
        const auto original=std::filesystem::path(argv[1]).wstring();
        const auto texture=std::filesystem::path(argv[2]).wstring();
        const auto oracle=[&]{CStorage data;assert(data.Load(original.c_str()));return VectorGeometry::Load(data);}();
        const auto expected_ranges=oracle.FrameRanges(0);
        assert(expected_ranges.size()==1);
        auto derived=[&](const wchar_t* path,std::wstring key,bool two=false) {
            CStorage data;assert(data.Load(original.c_str()));
            auto* names=data.GetBuf(L"surfs",L"texs",ST_WCHAR);assert(names && names->GetArraysCount()==1);
            names->SetWStr(0,key);if(two)names->AddWStr(key);
            data.Save(path);
        };
        derived(L"simple.vo",texture);derived(L"multi.vo",texture,true);
        derived(L"failure.vo",L"missing-skin.png");derived(L"gloss.vo",texture+L"*"+texture);
        derived(L"partial.vo",texture,true);
        {CStorage data;assert(data.Load(L"partial.vo"));data.GetBuf(L"surfs",L"texs",ST_WCHAR)->SetWStr(1,L"missing-second-skin.png");data.Save(L"partial.vo");}
        for(int cycle=0;cycle<2;++cycle) {
            NativeInitRuntime runtime;runtime.Begin(nullptr,nullptr);
            NativeSkinLibrary skins(runtime,true);NativeSkinPipeline pipeline(runtime,skins);
            auto object=[&](const wchar_t* name){return static_cast<CVectorObject*>(g_Cache->Get(CacheClass::VO,name));};
            auto* single=object(L"simple.vo");
            pipeline.Load(*single,OLF_MULTIMATERIAL_ONLY,GSP_ORDINAL);assert(single->IsNoSkin(0));
            auto fallback=pipeline.AcquireSkin(texture.c_str(),GSP_ORDINAL);
            auto batch=pipeline.Prepare(*single,0,{},fallback);
            assert(batch.stages==1 && batch.set_texture==1 && batch.clear==1 && batch.preload==0);
            assert(batch.submissions.size()==1 && batch.textures.size()==1);
            for(auto mode:{OLF_SIMPLE,OLF_AUTO}) {
                pipeline.Load(*single,mode,GSP_ORDINAL);assert(!single->IsNoSkin(0));
                batch=pipeline.Prepare(*single,0,{});
                assert(batch.preload==1 && batch.stages==1 && batch.clear==1);
                assert(batch.submissions.size()==expected_ranges.size());
                const auto& actual=batch.submissions[0].range;const auto& expected=expected_ranges[0];
                std::printf("frame index_count original Draw=%u independent STRG reader=%u full index storage=%zu\n",
                    actual.index_count,expected.index_count,oracle.Indices().size());
                assert(actual.index_count==expected.index_count && actual.surface==expected.surface &&
                    actual.minimum_vertex==expected.minimum_vertex && actual.vertex_count==expected.vertex_count);
            }
            auto* multi=object(L"multi.vo");pipeline.Load(*multi,OLF_MULTIMATERIAL_ONLY,GSP_ORDINAL);
            assert(!multi->IsNoSkin(0) && !multi->IsNoSkin(1));
            batch=pipeline.Prepare(*multi,0,{});
            assert(batch.preload==2 && batch.stages==2 && batch.set_texture==2 && batch.clear==2);
            assert(batch.submissions.size()==1 && batch.textures.size()==1);
            const auto geometry=buffers::CaptureBoundVO(buffers::Current(),std::span(&batch.submissions[0].range,1));
            assert(!geometry.vertices.empty() && !geometry.indices.empty());
            auto* partial=object(L"partial.vo");
            rejected([&]{pipeline.Load(*partial,OLF_AUTO,GSP_ORDINAL);});
            assert(!partial->IsLoaded() && !SkinProviderBound());
#ifdef MATRIXGAME_CALLBACK_ALLOC_FAULTS
            {
                AllocationFailureProvider failure_provider(pipeline);allocation_hit=false;
                SkinLoadScope load_scope(failure_provider);
                rejected([&]{multi->LoadSpecial(OLF_AUTO,AcquireOwnedSkin,GSP_ORDINAL);});
                fail_new=false;assert(allocation_hit && failure_provider.calls==2 && !multi->IsLoaded());
            }
            assert(!SkinProviderBound());pipeline.Load(*multi,OLF_AUTO,GSP_ORDINAL);
            assert(pipeline.Prepare(*multi,0,{}).submissions.size()==1);
            std::puts("PASS actual allocation failure at second skin callback; partial load rollback/retry");
#endif
            rejected([&]{pipeline.Prepare(*multi,multi->GetFramesCnt(),{});});
            assert(!InspectNativeDrawSink() && !SkinProviderBound());
            auto* failure=object(L"failure.vo");
            rejected([&]{pipeline.Load(*failure,OLF_AUTO,GSP_ORDINAL);});assert(!failure->IsLoaded());
            assert(!SkinProviderBound());
            // Repair only this test's private derived resource, then retry the same object.
            derived(L"failure.vo",texture);pipeline.Load(*failure,OLF_AUTO,GSP_ORDINAL);
            assert(failure->IsLoaded() && !failure->IsNoSkin(0));
            auto* gloss=object(L"gloss.vo");pipeline.Load(*gloss,OLF_AUTO,GSP_ORDINAL);
            rejected([&]{pipeline.Prepare(*gloss,0,{});});assert(!InspectNativeDrawSink());
            // Drop the catalogue while models retain raw SSkin fields: ownership
            // leases keep those objects alive, expired resources fail explicitly.
            skins.Clear();rejected([&]{pipeline.Prepare(*multi,0,{});});
            pipeline.Load(*multi,OLF_AUTO,GSP_ORDINAL);batch=pipeline.Prepare(*multi,0,{});
            assert(batch.preload==2 && batch.submissions.size()==1);
            fallback.reset();runtime.Deinit();skins.Clear();
            assert(!g_Cache && !g_CacheHeap && !CachedTextureBytes());
            assert(!SkinProviderBound() && !InspectNativeDrawSink());
            // Next cycle's failure fixture starts invalid again.
            derived(L"failure.vo",L"missing-skin.png");
            std::printf("PASS cycle=%d original LoadSpecial SIMPLE/AUTO/MULTIMATERIAL_ONLY -> BeforeDraw -> Draw callbacks; fallback/multi/rollback/ownership/cleanup\n",cycle);
        }
        std::puts("SKIN_CALLBACK_RUNTIME_PASS; native ordinal shader contract; GPU HARDWARE_PENDING; CGame::Init NOT CALLED");
    }catch(const Base::CException&){std::fputs("FAIL MatrixLib exception\n",stderr);return 1;}
     catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
