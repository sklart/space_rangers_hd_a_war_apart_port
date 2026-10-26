// Compile/link proof only; actual Switch Init/map/frame execution is pending.
#include "platform_switch.hpp"
#include "init_runtime.hpp"
#include "applet_window_lifecycle.hpp"
#include "MatrixMap.hpp"
#include "MatrixFormGame.hpp"
#include "native_loop_switch.hpp"
#include <switch.h>
#include <cstdio>
#include <codecvt>
#include <locale>
int main(int argc,char** argv){
    using namespace srhd_awa::matrixgame;
    try{
        const char* root_arg=argc>1 && argv[1] && *argv[1]?argv[1]:"sdmc:/switch/space-rangers-hd-a-war-apart/game";
        const auto resource_root=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(root_arg);
        std::printf("native MatrixGame resource_root=%s\n",root_arg);
        // This process owns Npad policy. SwitchInput itself only borrows it:
        // a host integrating MatrixGame must retain its own controller setup.
        // Same setup order as libnx padConfigureInput, but propagate the IPC
        // failure through the existing exception cleanup instead of aborting.
        if(!serviceIsActive(hidGetServiceSession()) || !hidGetSharedmemAddr())
            throw platform3g::Unsupported("standalone input requires live libnx HID");
        hidInitializeNpad();
        const HidNpadIdType ids[]{HidNpadIdType_No1,HidNpadIdType_Handheld};
        platform3g::CheckDeviceResult(static_cast<platform3g::DeviceResult>(hidSetSupportedNpadIdType(ids,2)),"standalone Npad player IDs");
        // Pro, paired Joy-Con and handheld expose both sticks and all controls
        // used by the existing original-key/cursor adapter.
        platform3g::CheckDeviceResult(static_cast<platform3g::DeviceResult>(hidSetSupportedNpadStyleSet(HidNpadStyleSet_NpadFullCtrl)),"standalone Npad controller styles");
        // This standalone process explicitly establishes the visibility baseline.
        // A borrowing host must instead supply its own known prior visibility.
        platform3g::CheckDeviceResult(static_cast<platform3g::DeviceResult>(appletSetAppletWindowVisibility(true)),"establish applet visibility baseline");
        platform3g::SwitchWindow window(*nwindowGetDefault(),true);
        platform3g::DekoDevice device(window);
        NativeInitRuntime runtime;
        NativeGameOptions options{window,device};options.seed=37;
        options.draw_sink=&device;
        options.output_root=L"sdmc:/switch/srhd-awa-matrixgame-native"; // Dedicated native output tree, never original resources/saves.
        options.resource_root=resource_root.c_str();
        runtime.InitializeNativeGame(options); // Complete original CGame::Init, not a demo scene.
        {
            native_form::Session form;
            native_loop::SwitchBackend backend(window,device);native_loop::Scope loop(backend);
            // Applet focus is authoritative; do not force APPACTIVE while the
            // console is suspended. Original L3GRun handles inactive/no-form.
            g_ExitState=0;g_Flags&=~GFLAG_EXITLOOP;
            RunOriginalNativeLoop();
        } // Original form Leave/destructor runs while map/platform are alive.
        runtime.Deinit();platform3g::CheckDeviceResult(static_cast<platform3g::DeviceResult>(window.LastVisibilityRestoreResult()),"restore applet visibility");return 0;
    }catch(const applet_window::ExitRequested&){std::puts("native applet exit requested; runtime unwound");return 0;}
    catch(const std::exception& error){std::printf("native original Init stopped: %s\n",error.what());return 10;}
}
