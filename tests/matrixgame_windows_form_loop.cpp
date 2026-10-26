// SPDX-License-Identifier: GPL-2.0-or-later
// Real HAL Init, complete original form/vtable and original loop. The platform
// event/clock fixture supplies bounded, reproducible input; it is not OS HID.
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "MatrixMap.hpp"
#include "MatrixFormGame.hpp"
#include "MatrixConfig.hpp"
#include "native_form_backend.hpp"
#include "native_loop.hpp"
#include "native_input_engine.hpp"
#include "applet_engine_events.hpp"
#include "skin_callbacks.hpp"
#include "platform_win32.hpp"
#include "offscreen_backend.hpp"
#include "input.hpp"
#include "CException.hpp"
#include "resource_paths.hpp"
#include <cstdio>
#include <codecvt>
#include <locale>
void DrawOriginalWindowsFrame(void (*)());
void CaptureOriginalMapFrame();
void VerifyOriginalStoredSlots(srhd_awa::matrixgame::platform3g::Device&);
namespace {
namespace mg=srhd_awa::matrixgame;
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
struct Driver final:mg::native_loop::Backend {
    mg::native_input::EngineSink input;mg::platform3g::AppletEngineEvents events;
    unsigned pumps{},draws{};float camera_x{},camera_y{};int start_time{};
    void Key(std::uint8_t key){input.Key(true,key);input.Key(false,key);}
    void Pump()override{
        ++pumps;
        switch(pumps){
        case 1:events.FocusChanged(true);input.Move(400,300);Require(g_MatrixMap->IsPaused(),"begin dialog not paused");start_time=g_MatrixMap->GetTime();break;
        case 2:Key(13);Require(!g_MatrixMap->IsPaused(),"original Keyboard Enter did not accept begin");break;
        case 3:Key(19);Require(g_MatrixMap->IsPaused(),"original Keyboard Pause did not pause");break;
        case 4:Key(19);Require(!g_MatrixMap->IsPaused(),"original Keyboard Pause did not resume");break;
        case 5:Key(27);Require(g_MatrixMap->IsPaused(),"original Keyboard Escape did not open menu");break;
        case 6:Key(27);Require(!g_MatrixMap->IsPaused(),"original Keyboard Escape did not close menu");break;
        case 7:{const auto xy=g_MatrixMap->m_Camera.GetXYStrategy();camera_x=xy.x;camera_y=xy.y;const auto key=g_Config.m_KeyActions[KA_SCROLL_RIGHT];Require(key!=0,"unbound scroll key");input.Key(true,key);Require(Input::isKeyPressed(KA_SCROLL_RIGHT),"original scroll key not held");break;}
        case 8:input.Key(false,g_Config.m_KeyActions[KA_SCROLL_RIGHT]);Require(!Input::isKeyPressed(KA_SCROLL_RIGHT),"original scroll key stuck");break;
        case 9:g_Flags|=GFLAG_EXITLOOP;break;
        default:throw std::runtime_error("unbounded original loop");
        }
    }
    void Idle()override{throw std::runtime_error("controlled HAL form unexpectedly inactive");}
    void Draw(CForm& form)override{form.Draw();++draws;}
    mg::native_loop::Clock::time_point Now()override{return mg::native_loop::Clock::time_point{}+std::chrono::milliseconds(pumps*25);}
};
}
int RunOriginalWindowsGame(void* module,void* hwnd,std::uintptr_t api,std::uintptr_t raw,
    mg::platform3g::Window& window,mg::platform3g::Device& device){
    using namespace mg;
    output_paths::NativeScope private_outputs(MATRIXGAME_FORM_TEST_OUTPUT);
    auto settings=SRobotsSettings::generate_default_settings();settings.FDirect3D=reinterpret_cast<void*>(api);settings.FD3DDevice=reinterpret_cast<void*>(raw);
    settings.m_ResolutionX=800;settings.m_ResolutionY=600;
    NativeInitRuntime runtime;OriginalGameOptions options;options.module=module;options.window=hwnd;options.seed=37;options.window_provider=&window;options.device_provider=&device;options.settings=&settings;
#ifdef MATRIXGAME_FORM_TEST_RESOURCE_ROOT
    options.resource_root=MATRIXGAME_FORM_TEST_RESOURCE_ROOT;const auto original_cwd=std::filesystem::current_path();
#endif
    try{
#ifdef MATRIXGAME_FORM_TEST_RESOURCE_ROOT
        {
            NativeInitRuntime failed;auto missing=options;const auto empty_root=std::filesystem::current_path().wstring();missing.resource_root=empty_root.c_str();
            bool rejected=false;try{failed.InitializeOriginalGame(missing);}catch(const Base::CException&){rejected=true;}
            Require(rejected && !failed.OriginalGameReady() && !failed.CpuReady() && !resource_paths::Bound() && !g_MatrixHeap && !g_MatrixData && !g_Cache && !g_MatrixMap,"early resource failure leaked original runtime/root owners");
            Require(std::filesystem::current_path()==original_cwd,"resource failure changed cwd");
            std::puts("REAL_RESOURCE_EARLY_FAILURE package_open_rejected=PASS original_cleanup=PASS root_cleanup=PASS");
        }
#endif
        runtime.InitializeOriginalGame(options);Require(runtime.OriginalGameReady(),"original Init not ready");
        std::puts("original CGame::Init returned: ready=1");
#ifdef MATRIXGAME_FORM_TEST_RESOURCE_ROOT
        Require(resource_paths::Bound() && std::filesystem::current_path()==original_cwd,"resource root changed cwd or was not retained");
#endif
        VerifyOriginalStoredSlots(device);
        auto& sdk=dynamic_cast<platform3g::D3D9Device&>(device);const auto before=sdk.InspectDrawStats();
        {
            native_form::Session form;Require(g_FormCur==&form.Form(),"original FormChange did not select form");
            Driver backend;native_loop::Scope scope(backend);g_Flags&=~GFLAG_EXITLOOP;
            Require(RunOriginalNativeLoop()==1,"original loop return");
            Require(backend.draws==8 && g_MatrixMap->GetTime()-backend.start_time==125,"original loop simulation time/draw count");
            const auto xy=g_MatrixMap->m_Camera.GetXYStrategy();Require(xy.x!=backend.camera_x || xy.y!=backend.camera_y,"original scroll action did not move camera");
            Require(g_MatrixMap->m_Cursor.GetPosX()==400 && g_MatrixMap->m_Cursor.GetPosY()==300,"original MouseMove did not update cursor");
            // Explicit optional-path guard: no fake screenshot/save success.
            bool unsupported=false;try{backend.Key(120);}catch(const platform3g::Unsupported&){unsupported=true;}backend.input.Key(false,120);Require(unsupported,"native F9 must explicitly reject missing screenshot provider");
            backend.events.FocusChanged(false);
            Require(!(g_Flags&GFLAG_APPACTIVE) && !(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY),"original SystemEvent did not release video resources");
            backend.events.FocusChanged(true);
            form.Form().Takt(25); // Genuine CheckLostDevice -> CreatePoolDefaultResources(false).
            Require(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY,"original recovery did not recreate video resources");
#ifdef MATRIXGAME_FORM_TEST_RESOURCE_ROOT
            Require(resource_paths::Bound() && std::filesystem::current_path()==original_cwd,"resource root/cwd changed during original recovery");
#endif
            form.Form().Draw();
            // Observe a final genuine original Draw before Present; no shader,
            // visibility or simulation path is replaced for the readback.
            DrawOriginalWindowsFrame(CaptureOriginalMapFrame);
            std::printf("REAL_FORM_LOOP draws=8 elapsed=125 Enter/Pause/Escape/scroll/MouseMove/F9_guard=PASS focus_release_recreate=PASS FPS=%d\n",g_DrawFPS);
        }
        Require(!g_FormCur && !g_FormFirst && !g_FormLast,"original form owner leaked");
        const auto after=sdk.InspectDrawStats();Require(after.indexed>before.indexed,"original form made no actual indexed draws");
        std::printf("actual form HAL draws=%llu indexed=%llu\n",after.calls-before.calls,after.indexed-before.indexed);
        runtime.Deinit();Require(runtime.LastStateRestoreResult()==platform3g::DeviceSuccess,"device restore failed");
#ifdef MATRIXGAME_FORM_TEST_RESOURCE_ROOT
        Require(!resource_paths::Bound() && std::filesystem::current_path()==original_cwd,"resource root/cwd leaked after Deinit");
        std::puts("REAL_RESOURCE_SCOPE full_Init_PrepareMap=PASS foreign_cwd_unchanged=PASS root_through_recovery=PASS cleanup=PASS");
#endif
        Require(!g_MatrixMap && !g_MatrixHeap && !BorrowedSkinLeaseCount() && !InspectNativeDrawSink(),"original game owner leaked");
        return 0;
    }catch(const Base::CException& error){std::printf("original form CException: %s\n",std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(error.Info()).c_str());}
    catch(const std::exception& error){std::printf("original form boundary: %s\n",error.what());}
    offscreen::EndScene();runtime.Deinit();return 10;
}
