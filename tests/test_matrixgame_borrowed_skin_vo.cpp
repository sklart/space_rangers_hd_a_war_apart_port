// Real original VO loader/BeforeDraw with a deliberately freed fixture skin.
#include "VectorObjectCpu.hpp"
#include <cassert>
#include <filesystem>
#include <memory>
#include <cstdio>
Base::CHeap* g_CacheHeap{};
using namespace srhd_awa::matrixgame;
static std::unique_ptr<SSkin> skin;
static unsigned preloads{};
static void Preload(const SSkin* value){assert(value==skin.get());++preloads;}
static const SSkin* Get(const wchar_t*,std::uint32_t){assert(skin);return skin.get();}
template<class F> static void Refused(F f){bool rejected=false;try{f();}catch(const std::logic_error&){rejected=true;}assert(rejected);}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    Base::CHeap heap;g_CacheHeap=&heap;
    buffers::Context context;buffers::Scope scope(context);
    for(int cycle=0;cycle<2;++cycle){
        CVectorObject::StaticInit();
        {
            CVectorObject object;object.m_Name=std::filesystem::path(argv[1]).wstring();
            Refused([&]{object.LoadSpecial(OLF_AUTO,Get,0);});assert(!object.IsLoaded());
            {
                BorrowedSkinScope manager(Get);
                for(int retry=0;retry<2;++retry){
                    skin=std::make_unique<SSkin>();skin->m_Preload=Preload;
                    object.LoadSpecial(OLF_AUTO,Get,0);assert(object.IsLoaded() && !object.IsNoSkin(0));
                    const auto before=preloads;object.BeforeDraw();assert(preloads>before);
                    InvalidateBorrowedSkins(Get);skin.reset(); // actually free the pointed-to skin
                    Refused([&]{object.BeforeDraw();});
                    Refused([&]{object.Draw(0,0,nullptr);});
                    assert(preloads>before);object.Unload();assert(!BorrowedSkinLeaseCount());
                }
                skin=std::make_unique<SSkin>();skin->m_Preload=Preload;
                object.LoadSpecial(OLF_AUTO,Get,0);
            }
            skin.reset();Refused([&]{object.BeforeDraw();});object.Unload();
        }
        assert(!BorrowedSkinLeaseCount() && !context.AllocatedBytes());
    }
    g_CacheHeap=nullptr;
    std::printf("original VO LoadSpecial/BeforeDraw, actual freed borrowed skin rejected before callback/Draw, invalidation/retry/Unload/scope-end/reentry PASS; preloads=%u; original CSkinManager tested separately in Windows Init\n",preloads);
}
