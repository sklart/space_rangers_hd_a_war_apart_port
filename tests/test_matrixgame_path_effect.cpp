// Real factory/CBuf/billboards/heap, with forced moving realloc for ownership.
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "Effects/MatrixEffect.hpp"
#include "Effects/MatrixEffectPath.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <malloc.h>
#include <algorithm>

static unsigned moves{};
static void* fail_pointer{};
static unsigned failures{};
struct MatrixPathTestAccess {
    static void* Data(CMatrixEffectPath* p){return p->m_Dots;}
    static void Check(CMatrixEffectPath* p){
        if(!p->m_DotsCnt){assert(!p->m_First && !p->m_Last);return;}
        assert(p->m_First && p->m_Last && !p->m_First->prev && !p->m_Last->next);
        int count=0;SPathDot* previous=nullptr;
        for(auto* node=p->m_First;node;node=node->next){
            assert(node>=p->m_Dots && node<p->m_Dots+p->m_DotsCnt);
            assert(node->prev==previous);previous=node;assert(++count<=p->m_DotsCnt);
        }
        assert(count==p->m_DotsCnt && previous==p->m_Last);
    }
};
extern "C" void* __real_realloc(void*,std::size_t);
extern "C" void* __wrap_realloc(void* old,std::size_t bytes){
    if(old && old==fail_pointer){fail_pointer=nullptr;++failures;return nullptr;}
    if(!old)return std::malloc(bytes);
    if(!bytes){std::free(old);return nullptr;}
    auto* next=std::malloc(bytes);
    if(!next)return nullptr;
#ifdef _WIN32
    const auto old_size=_msize(old);
#else
    const auto old_size=malloc_usable_size(old);
#endif
    std::memcpy(next,old,std::min(bytes,old_size));
    std::free(old);++moves;
    return next;
}
extern "C" void* matrix_path_realloc(void* old,std::size_t bytes){return __wrap_realloc(old,bytes);}
static void Position(CMatrixEffectPath* effect,float t,float x,float y,float z){
    D3DXVECTOR3 value;effect->GetPos(&value,t);
    assert(std::fabs(value.x-x)<0.0001f && std::fabs(value.y-y)<0.0001f && std::fabs(value.z-z)<0.0001f);
}
int main(){
    std::setvbuf(stdout,nullptr,_IONBF,0);
    using srhd_awa::matrixgame::NativeInitRuntime;
    NativeInitRuntime runtime;runtime.Begin(nullptr,nullptr);
    assert(g_MatrixMap==nullptr);
    for(int cycle=0;cycle<2;++cycle){
        const D3DXVECTOR3 points[]={{0,0,0},{30,0,0},{30,40,0}};
        auto* base=CMatrixEffect::CreatePath(points,3);
        auto* path=dynamic_cast<CMatrixEffectPath*>(base);assert(path);
        struct Owner{CMatrixEffect* value;~Owner(){value->Release();}} owner{base};
        Position(path,0.5f,30,5,0);
        MatrixPathTestAccess::Check(path);
        auto* empty_data=MatrixPathTestAccess::Data(path);
        fail_pointer=empty_data;
        bool empty_rejected=false;
        try{path->AddPos({30,40,50});}catch(...){empty_rejected=true;}
        assert(empty_rejected && !fail_pointer && MatrixPathTestAccess::Data(path)==empty_data);
        MatrixPathTestAccess::Check(path);
        std::puts("original path factory ready; growing before first Takt (empty list)");
        path->AddPos({30,40,50});
        MatrixPathTestAccess::Check(path);
        Position(path,0.5f,30,30,0);
        path->BeforeDraw();path->Takt(1);
        std::puts("original path first Takt ready; growing populated billboard list");
        path->AddPos({130,40,50});
        MatrixPathTestAccess::Check(path);
        Position(path,0.75f,75,40,50);
        Position(path,1.75f,75,40,50);
        Position(path,-0.25f,75,40,50);
        path->Takt(20);path->Takt(5);
        MatrixPathTestAccess::Check(path);
        auto* data=MatrixPathTestAccess::Data(path);
        fail_pointer=data;
        bool rejected=false;
        try{path->AddPos({230,40,50});}catch(...){rejected=true;}
        assert(rejected && !fail_pointer && MatrixPathTestAccess::Data(path)==data);
        MatrixPathTestAccess::Check(path);
        path->AddPos({330,40,50}); // retry after allocator failure
        MatrixPathTestAccess::Check(path);
        assert(g_MatrixMap==nullptr);
    }
    assert(moves>0 && failures==4);runtime.Deinit();
    std::printf("original path factory, AddPos empty/populated moving allocation, GetPos scalar positions, Takt billboards, Release/reentry PASS; moves=%u; map/GPU draw NOT RUN\n",moves);
}
