// Complete original CGame::Init, separately from the minimal bootstrap driver.
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "MatrixConfig.hpp"
#include "skin_callbacks.hpp"
#include "CException.hpp"
#include <cstdio>
#include <iostream>
#include <codecvt>
#include <locale>
int RunOriginalWindowsGame(void* module,void* hwnd,std::uintptr_t api,std::uintptr_t raw,
    srhd_awa::matrixgame::platform3g::Window& window,srhd_awa::matrixgame::platform3g::Device& device){
    using namespace srhd_awa::matrixgame;
    auto settings=SRobotsSettings::generate_default_settings();
    settings.FDirect3D=reinterpret_cast<void*>(api);settings.FD3DDevice=reinterpret_cast<void*>(raw);
    settings.m_ResolutionX=640;settings.m_ResolutionY=480;
    NativeInitRuntime runtime;
    OriginalGameOptions options;options.module=module;options.window=hwnd;options.seed=37;
    options.window_provider=&window;options.device_provider=&device;options.settings=&settings;
    std::puts("calling complete original CGame::Init with actual HAL Window/Device providers");
    try{
        runtime.InitializeOriginalGame(options);
        std::printf("original CGame::Init returned: ready=%d map=%p\n",runtime.OriginalGameReady(),static_cast<void*>(g_MatrixMap));
        runtime.Deinit();return 0;
    }catch(const Base::CException& error){
        std::cout<<"original Init CException: "<<std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(error.Info())<<'\n';
    }catch(const std::exception& error){std::printf("original Init boundary: %s\n",error.what());}
    std::printf("original Init stopped; after cleanup map=%p heap=%p\n",static_cast<void*>(g_MatrixMap),static_cast<void*>(g_MatrixHeap));
    if(BorrowedSkinLeaseCount())return 13;
    std::puts("original borrowed skin leases empty after cleanup");
    return 10;
}
