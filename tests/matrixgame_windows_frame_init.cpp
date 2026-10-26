// Complete original CGame::Init, separately from the minimal bootstrap driver.
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "MatrixConfig.hpp"
#include "skin_callbacks.hpp"
#include "CException.hpp"
#include "MatrixMap.hpp"
#include "platform_win32.hpp"
#include "offscreen_backend.hpp"
#include <cstdio>
#include <iostream>
#include <codecvt>
#include <locale>
void DrawOriginalWindowsFrame(void (*)());
void CaptureOriginalMapFrame();
int RunOriginalWindowsGame(void* module,void* hwnd,std::uintptr_t api,std::uintptr_t raw,
    srhd_awa::matrixgame::platform3g::Window& window,srhd_awa::matrixgame::platform3g::Device& device){
    using namespace srhd_awa::matrixgame;
    auto settings=SRobotsSettings::generate_default_settings();
    settings.FDirect3D=reinterpret_cast<void*>(api);settings.FD3DDevice=reinterpret_cast<void*>(raw);
    settings.m_ResolutionX=800;settings.m_ResolutionY=600;
    NativeInitRuntime runtime;
    OriginalGameOptions options;options.module=module;options.window=hwnd;options.seed=37;
    options.window_provider=&window;options.device_provider=&device;options.settings=&settings;
    std::puts("calling complete original CGame::Init with actual HAL Window/Device providers");
    try{
        runtime.InitializeOriginalGame(options);
        std::printf("original CGame::Init returned: ready=%d map=%p\n",runtime.OriginalGameReady(),static_cast<void*>(g_MatrixMap));
        const auto client=window.ReadClientRect();
        if(client.right-client.left!=800 || client.bottom-client.top!=600)throw std::runtime_error("actual client size mismatch");
        std::printf("actual client after Init %dx%d\n",client.right-client.left,client.bottom-client.top);
        if(g_MatrixMap->m_Size.x!=276 || g_MatrixMap->m_Size.y!=285 ||
           g_MatrixMap->m_GroupSize.x!=28 || g_MatrixMap->m_GroupSize.y!=29)throw std::runtime_error("original map dimensions changed");
        auto& sdk=dynamic_cast<platform3g::D3D9Device&>(device);
        const auto before=sdk.InspectDrawStats();
        std::puts("enter original CFormMatrixGame::Draw chain");
        DrawOriginalWindowsFrame(CaptureOriginalMapFrame);
        const auto after=sdk.InspectDrawStats();
        if(after.calls<=before.calls || after.indexed<=before.indexed)throw std::runtime_error("no actual original map draws");
        std::printf("original frame returned; actual SDK draws=%llu indexed=%llu primitives=%llu\n",
            after.calls-before.calls,after.indexed-before.indexed,after.primitives-before.primitives);
        runtime.Deinit();
        if(runtime.LastStateRestoreResult()!=platform3g::DeviceSuccess)throw std::runtime_error("original device state restore failed");
        if(g_MatrixMap || g_MatrixHeap || BorrowedSkinLeaseCount() || InspectNativeDrawSink())throw std::runtime_error("original game cleanup leaked owner");
        return 0;
    }catch(const Base::CException& error){
        std::cout<<"original Init CException: "<<std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(error.Info())<<'\n';
    }catch(const std::exception& error){std::printf("original Init boundary: %s\n",error.what());}
    // A failure after the original BeginScene must not strand the owned SDK
    // scene while the diagnostic runner unwinds the full engine.
    srhd_awa::matrixgame::offscreen::EndScene();
    runtime.Deinit();
    std::printf("original Init/frame stopped; after cleanup map=%p heap=%p\n",static_cast<void*>(g_MatrixMap),static_cast<void*>(g_MatrixHeap));
    if(BorrowedSkinLeaseCount())return 13;
    std::puts("original borrowed skin leases empty after cleanup");
    return 10;
}
