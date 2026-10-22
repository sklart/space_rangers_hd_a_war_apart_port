// New cache/VO submission integration. No replay of component suites.
#include "cached_frame.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
using namespace srhd_awa::matrixgame;
#ifdef MATRIXGAME_SESSION_GPU_LINK
extern "C" void MatrixGameNativeSessionDraw(const wchar_t*,const wchar_t*);
#endif
template<class F> void rejected(F&& f) {bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(int argc,char** argv) {
#ifdef MATRIXGAME_SESSION_GPU_LINK
    auto volatile keep=&MatrixGameNativeSessionDraw;(void)keep;
#endif
    if(argc!=4)return 2;
    try {
        const auto model=std::filesystem::path(argv[1]).wstring();
        const auto texture=std::filesystem::path(argv[2]).wstring();
        const auto second_model=std::filesystem::path(argv[3]).wstring();
        for(int cycle=0;cycle<2;++cycle) {
            NativeInitRuntime runtime;runtime.Begin(nullptr,nullptr);
            auto* object=static_cast<CVectorObject*>(g_Cache->Get(CacheClass::VO,model));
            object->Prepare();
            auto material=BorrowMaterialTexture(*g_Cache->Get(CacheClass::TextureManaged,texture));
            std::vector<CachedFrameMaterial> materials(object->NativeSurfaceCount(),{material,{{},1,{},{}}});
            rejected([&]{PrepareCachedFrame(runtime,*object,-1,materials);});
            rejected([&]{PrepareCachedFrame(runtime,*object,object->GetFramesCnt(),materials);});
            rejected([&]{PrepareCachedFrame(runtime,*object,0,{});});
            materials[0].state.texture_slot=0;
            rejected([&]{PrepareCachedFrame(runtime,*object,0,materials);});
            materials[0].state.texture_slot=1;
            auto frame=PrepareCachedFrame(runtime,*object,0,materials);
            assert(!frame.submissions.empty() && frame.textures.size()==materials.size());
            std::vector<MeshRange> ranges;for(const auto& s:frame.submissions)ranges.push_back(s.range);
            const auto snapshot=buffers::CaptureBoundVO(buffers::Current(),ranges);
            assert(!snapshot.vertices.empty() && !snapshot.indices.empty());
            // Interleave another cached original object in the same shared VB/IB.
            auto* second=static_cast<CVectorObject*>(g_Cache->Get(CacheClass::VO,second_model));
            second->Prepare();
            auto second_frame=PrepareCachedFrame(runtime,*second,0,materials);
            assert(second_frame.submissions.size()==frame.submissions.size());
            assert(second_frame.submissions[0].range.base_vertex!=frame.submissions[0].range.base_vertex);
            auto again=PrepareCachedFrame(runtime,*object,0,materials);
            assert(again.submissions[0].range.base_vertex==frame.submissions[0].range.base_vertex);
            // Force the existing release/recreate path while keeping cache owners.
            CVectorObject::MarkAllBuffersNoNeed();
            again=PrepareCachedFrame(runtime,*object,0,materials);
            assert(!again.submissions.empty());
            auto stale=material;
            runtime.Deinit();
            assert(stale.Expired() && !g_Cache && !g_CacheHeap && !CachedTextureBytes());
            assert(!frame.textures[0]->levels.empty());
            std::printf("PASS cycle=%d cache VO -> native frame -> bound geometry; vertices=%zu indices=%zu draws=%zu; reentry/lease/cleanup\n",
                cycle,snapshot.vertices.size(),snapshot.indices.size(),frame.submissions.size());
        }
        std::puts("CACHED_FRAME_HOST_PASS; GPU/Horizon HARDWARE_PENDING; CGame::Init NOT CALLED");
    }catch(const Base::CException&){std::fputs("FAIL MatrixLib exception\n",stderr);return 1;}
     catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
