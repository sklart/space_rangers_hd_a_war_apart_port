// SPDX-License-Identifier: GPL-2.0-or-later
// Experimental full E2E516 startup with the existing shared native presenter.
// This is not the completed SDL/deko handoff; see the integration report.
#include "native_rangers_interface.hpp"
#include "native_loop_switch.hpp"
#include "platform_switch.hpp"
#include "rangers_host_callbacks.hpp"
#include "ec_file_adapter.hpp"
#include "user_root.hpp"
#include "e2e515_perf.hpp"
#include "units/Rangers.hpp"
#include "units/Robot.hpp"
#include "units/GR_Main.hpp"
#include <switch.h>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace mg=srhd_awa::matrixgame;
namespace p=mg::platform3g;
namespace platform=srhd_awa::platform;
namespace { mg::NativeRangersInterface* bridge{}; }
extern "C" void __real__ZN5Robot22InitializeRobotRuntimeEv();
extern "C" void __wrap__ZN5Robot22InitializeRobotRuntimeEv(){
    __real__ZN5Robot22InitializeRobotRuntimeEv();
    if(!bridge)throw std::logic_error("E2E516 native Robot bridge absent");
    if(!GR_Main::IsInstallFeatureEnabled(pas::WideString(u"Robot")))return;
    if(Robot::RobotInterface || Robot::RobotModule)
        throw std::logic_error("E2E516 Robot runtime already owns a module");
    bridge->Install(mg::MakeRangersHostCallbacks());
    std::puts("E2E516: native Robot interface installed");
}

int main(){
    try {
        if(appletGetAppletType()!=AppletType_Application && appletGetAppletType()!=AppletType_SystemApplication)
            throw std::runtime_error("application mode required");
        if(!serviceIsActive(hidGetServiceSession()) || !hidGetSharedmemAddr())
            throw std::runtime_error("libnx HID service required");
        hidInitializeNpad();
        const HidNpadIdType ids[]{HidNpadIdType_No1,HidNpadIdType_Handheld};
        p::CheckDeviceResult(static_cast<p::DeviceResult>(hidSetSupportedNpadIdType(ids,2)),"Npad player IDs");
        p::CheckDeviceResult(static_cast<p::DeviceResult>(hidSetSupportedNpadStyleSet(HidNpadStyleSet_NpadFullCtrl)),"Npad styles");
        p::CheckDeviceResult(static_cast<p::DeviceResult>(appletSetAppletWindowVisibility(true)),"applet visibility");
        constexpr auto game="sdmc:/switch/space-rangers-hd-a-war-apart/game";
        // A fresh isolated profile preserves any empty CFG.TXT left by the
        // previous std::filesystem::copy_file failure, without replacing it.
        constexpr auto user="sdmc:/switch/space-rangers-hd-a-war-apart/integrated-e2e516-test-v23";
        platform::ec_file::SetGameRoot(game);platform::ec_file::SetUserRoot(user);
        if(!platform::user_root::EnsureLayout())throw std::runtime_error("isolated user layout unavailable");
        if(!std::freopen("sdmc:/switch/space-rangers-hd-a-war-apart/integrated-e2e516-test-v23/logs/stdout.log","a",stdout) ||
           !std::freopen("sdmc:/switch/space-rangers-hd-a-war-apart/integrated-e2e516-test-v23/logs/stderr.log","a",stderr))
            throw std::runtime_error("isolated log files unavailable");
        std::setvbuf(stdout,nullptr,_IONBF,0);std::setvbuf(stderr,nullptr,_IONBF,0);
        std::error_code error;std::filesystem::current_path(game,error);
        if(error)throw std::runtime_error("game directory unavailable");
        // Detailed profiling is intentionally enabled for this diagnostic test.
        // An existing perf-mode.txt remains an explicit user override.
        platform::perf515::SetDefaultMode(2);
        platform::perf515::Session performance(user);
        p::SwitchDisplay display;p::SwitchWindow window(display.NativeWindow(),true,&display);p::DekoDevice device(window);
        mg::NativeGameOptions options{window,device};options.draw_sink=&device;
        options.resource_root=L"sdmc:/switch/space-rangers-hd-a-war-apart/game";
        options.output_root=L"sdmc:/switch/space-rangers-hd-a-war-apart/integrated-e2e516-test-v23/runtime/matrixgame";
        mg::NativeRangersInterface native(options);
        mg::native_loop::SwitchBackend battle_backend(window,device);
        mg::native_loop::Scope battle_loop(battle_backend);
        bridge=&native;
        struct ResetBridge {~ResetBridge(){bridge=nullptr;}} reset_bridge;
        std::puts("E2E516 integrated TEST: ProgramMain starting");
        Rangers::ProgramMain();
        std::puts("E2E516 integrated TEST: ProgramMain returned");
        return 0;
    } catch(const std::exception& error){
        std::fprintf(stderr,"E2E516 integrated TEST stopped: %s\n",error.what());return 1;
    } catch(...){std::fprintf(stderr,"E2E516 integrated TEST stopped: non-standard exception\n");return 1;}
}
