// Execute the real complete CGame::Init; no fabricated map/device/game result.
#include "init_runtime.hpp"
#include "cache_owner_state.hpp"
#include "MatrixGame.h"
#include "Texture.hpp"
#include "3g.hpp"
#include "CException.hpp"
#include "legacy_texture_backend.hpp"
#include <cassert>
#include <cstdio>
#include <codecvt>
#include <locale>
#include <iostream>
using namespace srhd_awa::matrixgame;
static std::string Utf8(const std::wstring& value){
    if constexpr(sizeof(wchar_t)==2)return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(value);
    else return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(value);
}
static void Empty(){
    assert(!g_MatrixHeap && !g_Cache && !g_CacheHeap && !g_MatrixData && !g_MatrixMap && !g_Render && !g_LoadProgress);
    assert(!cache_owner_detail::Owner() && !cache_owner_detail::OriginalGameOwner());
    assert(!platform3g::Current().ready && !InspectCpu3GState().bound);
    assert(!NativeCacheFactoryBound() && legacy_texture::RegistrySize()==0);
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    const bool real=std::string(argv[1])=="real";
    assert(real || std::string(argv[1])=="missing");
    {
        NativeInitRuntime cpu;cpu.Begin(nullptr,nullptr);
        auto* heap=g_MatrixHeap;auto* cache=g_Cache;
        bool init_refused=false,deinit_refused=false;
        try{CGame::Init(nullptr,nullptr,nullptr,37,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);}
        catch(const std::logic_error&){init_refused=true;}
        try{CGame::Deinit();}catch(const std::logic_error&){deinit_refused=true;}
        assert(init_refused && deinit_refused && g_MatrixHeap==heap && g_Cache==cache && cpu.CpuReady());
        cpu.Deinit();
        std::puts("direct Init/Deinit preserve existing CPU heap/cache owner PASS");
    }
    Empty();
    NativeInitRuntime game;
    for(int entry=0;entry<2;++entry){
        OriginalGameOptions options;options.seed=37;
        bool stopped=false,unsupported=false;
        try{game.InitializeOriginalGame(options);}
        catch(const platform3g::Unsupported& error){stopped=true;unsupported=true;std::cout<<"original Init boundary: "<<error.what()<<'\n';}
        catch(const Base::CException& error){stopped=true;std::cout<<"original Init resource failure: "<<Utf8(error.Info())<<'\n';}
        catch(const std::exception& error){std::cerr<<"unexpected original Init failure: "<<error.what()<<'\n';return 5;}
        if(!stopped || (real && !unsupported) || (!real && unsupported))return 6;
        assert(!game.CpuReady() && !game.OriginalGameReady());
        Empty();game.Deinit();game.Deinit();Empty();
        std::printf("original CGame::Init %s entry=%d -> original Deinit -> empty globals/cache/packages reentry PASS; battle NOT RUN\n",real?"real-resources":"missing-package",entry);
    }
}
