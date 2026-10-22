// Original no-light animator drives original VO callbacks/draw submissions.
#include "skin_pipeline.hpp"
#include "CStorage.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <numeric>
using namespace srhd_awa::matrixgame;
#ifdef MATRIXGAME_SESSION_GPU_LINK
extern "C" void MatrixGameNativeAnimationDraw(const wchar_t*);
#endif
template<class F> void rejected(F&& f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(int argc,char** argv) {
#ifdef MATRIXGAME_SESSION_GPU_LINK
    auto volatile keep=&MatrixGameNativeAnimationDraw;(void)keep;
#endif
    if(argc!=4)return 2;
    try {
        const auto source=std::filesystem::path(argv[1]).wstring();
        const auto texture=std::filesystem::path(argv[2]).wstring();
        const auto oracle=VectorGeometry::Load(source.c_str());
        std::size_t index=0;while(index<oracle.Animations().size() && oracle.Animations()[index].count<3)++index;
        assert(index<oracle.Animations().size());
        const auto& animation=oracle.Animations()[index];
        const auto frames=oracle.AnimationFrames().subspan(animation.first,animation.count);
        int period=0;for(const auto& f:frames){assert(f.time && f.time!=INT_MIN);period+=std::abs(f.time);}
        assert(period>0 && period<10000000);
        {CStorage derived;assert(derived.Load(source.c_str()));
            auto* names=derived.GetBuf(L"surfs",L"texs",ST_WCHAR);assert(names);
            for(unsigned i=0;i<names->GetArraysCount();++i)names->SetWStr(i,texture);
            derived.Save(L"animated.vo");}
        for(int cycle=0;cycle<2;++cycle) {
            NativeInitRuntime runtime;runtime.Begin(nullptr,nullptr);
            NativeSkinLibrary skins(runtime,false);NativeSkinPipeline pipeline(runtime,skins);
            auto* vo=static_cast<CVectorObject*>(g_Cache->Get(CacheClass::VO,L"animated.vo"));
            pipeline.Load(*vo,OLF_AUTO,GSP_ORDINAL);
            NativeVectorObjectAnim actor;
            rejected([&]{actor.Init(nullptr,nullptr);});
            actor.Init(vo,nullptr);actor.SetAnimByIndex(int(index),1);
            assert(actor.GetFrame()==0 && actor.GetVOFrame()==frames[0].frame);
            assert(!actor.Takt(std::abs(frames[0].time))); // Original strict > boundary.
            assert(actor.GetFrame()==0);
            assert(actor.Takt(1) && actor.GetFrame()==1 && actor.GetVOFrame()==frames[1].frame);
            const auto next_time=actor.NextFrame();assert(actor.GetFrame()==2 && actor.GetVOFrame()==frames[2].frame);
            assert(next_time==unsigned(std::abs(frames[2].time)));
            auto batch=pipeline.PrepareAnimated(actor,{});
            const auto ranges=oracle.FrameRanges(actor.GetVOFrame());
            assert(batch.submissions.size()==ranges.size() && !batch.submissions.empty());
            for(std::size_t i=0;i<ranges.size();++i) {
                assert(batch.submissions[i].range.index_count==ranges[i].index_count);
                assert(batch.submissions[i].range.surface==ranges[i].surface);
            }
            assert(batch.preload==std::size_t(vo->NativeSurfaceCount()) && batch.stages==batch.clear);
            D3DXVECTOR3 low{},high{};actor.GetBound(low,high);
            const auto& box=oracle.Frames()[actor.GetVOFrame()];
            assert(low.x==box.m_Min.x && low.y==box.m_Min.y && low.z==box.m_Min.z);
            assert(high.x==box.m_Max.x && high.y==box.m_Max.y && high.z==box.m_Max.z);
            // Missing name leaves the active animation/frame unchanged.
            const auto saved=actor.GetVOFrame();assert(actor.SetAnimByName(L"missing-animation"));
            assert(actor.GetVOFrame()==saved);
            // The original name/ID/default selectors are retained.
            assert(!actor.SetAnimByName(vo->GetAnimName(int(index)),1));
            actor.SetAnimById(animation.id,1);assert(actor.GetAnimIndex()==int(index));
            actor.SetAnimByIndex(int(index),0);
            for(unsigned i=0;i<animation.count+2;++i)actor.NextFrame();
            assert(actor.IsAnimEnd() && actor.GetFrame()==int(animation.count)-1);
            rejected([&]{actor.Takt(-1);});
            int light_token{};auto* unsupported=reinterpret_cast<CTextureManaged*>(&light_token);
            rejected([&]{actor.Init(vo,unsupported);});assert(actor.VO()==vo && actor.IsAnimEnd());
            NativeVectorObjectAnim catchup;catchup.Init(vo,nullptr);catchup.SetAnimByIndex(int(index),1);
            // A whole cycle returns to the same frame: original Takt returns false,
            // even though time advanced and a complete animation cycle elapsed.
            assert(!catchup.Takt(period+1) && catchup.GetFrame()==0 && catchup.GetVOFrame()==frames[0].frame);
            batch=pipeline.PrepareAnimated(catchup,{});assert(!batch.submissions.empty());
            catchup.Clear();rejected([&]{pipeline.PrepareAnimated(catchup,{});});
            actor.Clear();rejected([&]{actor.Takt(1);});
            // Borrowed VO remains loaded after original animator Clear.
            assert(vo->IsLoaded());
            pipeline.Load(*vo,OLF_NO_TEX,GSP_ORDINAL);
            auto fallback=pipeline.AcquireSkin(texture.c_str(),GSP_ORDINAL);
            actor.Init(vo,nullptr,fallback.get());actor.SetAnimDefault(1);
            batch=pipeline.PrepareAnimated(actor,{});assert(batch.preload==1 && !batch.submissions.empty());
            actor.Clear();fallback.reset();
            skins.Clear();runtime.Deinit();assert(!g_Cache && !g_CacheHeap && !CachedTextureBytes());
            assert(!InspectNativeDrawSink());
            std::printf("PASS cycle=%d original animation Takt/NextFrame/BeforeDraw/Draw; animation=%zu frames=%u period_ms=%d; frame=%d draws=%zu borrowed cleanup/reentry\n",
                cycle,index,animation.count,period,frames[2].frame,ranges.size());
        }
        std::puts("ANIMATION_RUNTIME_PASS; GPU HARDWARE_PENDING; billboard lights unsupported; CGame::Init NOT CALLED");
    }catch(const Base::CException&){std::fputs("FAIL MatrixLib exception\n",stderr);return 1;}
     catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
