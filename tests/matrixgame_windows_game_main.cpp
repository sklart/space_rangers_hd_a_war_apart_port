// SDK owner for the original Windows Init; the engine borrows both providers.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#include <cstdio>
#include <cstring>
static LONG CALLBACK TraceEngineException(EXCEPTION_POINTERS* info){
    if(info->ExceptionRecord->ExceptionCode==0xe06d7363u || info->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION){
        void* stack[24]{};const auto count=CaptureStackBackTrace(0,24,stack,nullptr);
        std::printf("exception code=%08lx address=%p stack image=%p:",info->ExceptionRecord->ExceptionCode,info->ExceptionRecord->ExceptionAddress,static_cast<void*>(GetModuleHandleW(nullptr)));
        for(USHORT i=0;i<count;++i)std::printf(" %p",stack[i]);
        std::puts("");
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void CheckVideoProviders(srhd_awa::matrixgame::platform3g::Win32Window& window,
    srhd_awa::matrixgame::platform3g::D3D9Device& device,void* module,IDirect3DDevice9* raw){
    namespace p=srhd_awa::matrixgame::platform3g;
    p::InitAsDLL(module,window,device);
    struct Session{~Session(){p::Deinit();}} session;
    D3DLIGHT9 original_light{};original_light.Type=D3DLIGHT_DIRECTIONAL;original_light.Direction.z=1.f;original_light.Diffuse.r=.25f;
    p::CheckDeviceResult(raw->SetLight(0,&original_light),"test baseline light");
    p::CheckDeviceResult(raw->LightEnable(0,FALSE),"test baseline light enable");
    auto state=device.CaptureState();
    D3DMATERIAL9 original{};D3DVIEWPORT9 old_viewport{};
    p::CheckDeviceResult(raw->GetMaterial(&original),"test GetMaterial before");
    p::CheckDeviceResult(raw->GetViewport(&old_viewport),"test GetViewport before");
    p::VideoMaterial material{{.1f,.2f,.3f,.4f},{.5f,.6f,.7f,.8f},{.9f,1.f,.25f,.75f},{.3f,.7f,.2f,.6f},13.5f};
    p::CheckDeviceResult(device.SetVideoMaterial(material),"test SetVideoMaterial");
    D3DMATERIAL9 actual{};p::CheckDeviceResult(raw->GetMaterial(&actual),"test GetMaterial");
    if(actual.Diffuse.r!=.1f || actual.Ambient.g!=.6f || actual.Specular.b!=.25f || actual.Emissive.a!=.6f || actual.Power!=13.5f)throw std::runtime_error("video material SDK readback mismatch");
    p::VideoViewport viewport{3,4,211,169,.125f,.875f};
    p::CheckDeviceResult(device.SetVideoViewport(viewport),"test SetVideoViewport");
    D3DVIEWPORT9 view{};p::CheckDeviceResult(raw->GetViewport(&view),"test GetViewport");
    if(view.X!=3 || view.Y!=4 || view.Width!=211 || view.Height!=169 || view.MinZ!=.125f || view.MaxZ!=.875f)throw std::runtime_error("video viewport SDK readback mismatch");
    const auto type=static_cast<srhd_awa::matrixgame::RasterRegister>(D3DRS_SPECULARMATERIALSOURCE);
    p::CheckDeviceResult(device.SetRenderState(type,D3DMCS_COLOR1),"test captured render register");
    if(device.ReadRenderState(type)!=D3DMCS_COLOR1)throw std::runtime_error("captured render state SDK readback mismatch");
    p::VideoLight light{};light.Type=D3DLIGHT_POINT;light.Diffuse={.1f,.2f,.3f,.4f};light.Ambient={.5f,.6f,.7f,.8f};
    light.Position={3,4,5};light.Direction={0,0,1};light.Range=123;light.Falloff=1;light.Attenuation0=1;light.Attenuation1=.1f;light.Attenuation2=.01f;light.Theta=.25f;light.Phi=.5f;
    p::CheckDeviceResult(device.SetVideoLight(0,light),"test SetVideoLight");
    p::CheckDeviceResult(device.EnableVideoLight(0,true),"test EnableVideoLight");
    D3DLIGHT9 actual_light{};BOOL enabled=FALSE;
    p::CheckDeviceResult(raw->GetLight(0,&actual_light),"test GetLight");p::CheckDeviceResult(raw->GetLightEnable(0,&enabled),"test GetLightEnable");
    if(!enabled || actual_light.Type!=D3DLIGHT_POINT || actual_light.Diffuse.g!=.2f || actual_light.Ambient.a!=.8f || actual_light.Position.y!=4 || actual_light.Direction.z!=1 || actual_light.Range!=123 || actual_light.Attenuation2!=.01f || actual_light.Phi!=.5f)throw std::runtime_error("video light SDK readback mismatch");
    p::CheckDeviceResult(state->Restore(),"test restore video contract");state.reset();
    p::CheckDeviceResult(raw->GetMaterial(&actual),"test restored GetMaterial");
    p::CheckDeviceResult(raw->GetViewport(&view),"test restored GetViewport");
    if(std::memcmp(&original,&actual,sizeof(actual)) || std::memcmp(&old_viewport,&view,sizeof(view)))throw std::runtime_error("video state restore readback mismatch");
    p::CheckDeviceResult(raw->GetLight(0,&actual_light),"test restored GetLight");p::CheckDeviceResult(raw->GetLightEnable(0,&enabled),"test restored GetLightEnable");
    if(enabled || std::memcmp(&original_light,&actual_light,sizeof(actual_light)))throw std::runtime_error("video light restore readback mismatch");
    std::puts("actual SDK material/viewport/light/register readback and restoration PASS");
}
int RunOriginalWindowsGame(void*,void*,std::uintptr_t,std::uintptr_t,
    srhd_awa::matrixgame::platform3g::Window&,srhd_awa::matrixgame::platform3g::Device&);
int main(){
    std::setvbuf(stdout,nullptr,_IONBF,0);
    auto* handler=AddVectoredExceptionHandler(1,TraceEngineException);
    struct HandlerOwner{void* value;~HandlerOwner(){if(value)RemoveVectoredExceptionHandler(value);}} own_handler{handler};
    const auto module=GetModuleHandleW(nullptr);
    const auto hwnd=CreateWindowExW(0,L"STATIC",L"MatrixGame original Init",WS_POPUP,0,0,640,480,nullptr,nullptr,module,nullptr);
    if(!hwnd){std::printf("CreateWindow failed %lu\n",GetLastError());return 77;}
    struct WindowOwner{HWND h;~WindowOwner(){DestroyWindow(h);}} own_window{hwnd};
    auto* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api)return 77;
    struct ApiOwner{IDirect3D9*p;~ApiOwner(){p->Release();}} own_api{api};
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=hwnd;pp.BackBufferWidth=640;pp.BackBufferHeight=480;
    IDirect3DDevice9* raw=nullptr;
    auto hr=api->CreateDevice(0,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&raw);
    std::printf("D3D9 HAL CreateDevice HRESULT=%08lx\n",hr);
    if(FAILED(hr))return 77;
    struct DeviceOwner{IDirect3DDevice9*p;~DeviceOwner(){p->Release();}} own_device{raw};
    namespace p=srhd_awa::matrixgame::platform3g;
    p::Win32Window window(hwnd);p::D3D9Device device(raw);
    CheckVideoProviders(window,device,module,raw);
    D3DGAMMARAMP before{},after{};raw->GetGammaRamp(0,&before);
    int result=0;
    for(int entry=0;entry<2;++entry){
        std::printf("original Init entry=%d\n",entry);
        result=RunOriginalWindowsGame(module,hwnd,reinterpret_cast<std::uintptr_t>(api),reinterpret_cast<std::uintptr_t>(raw),window,device);
        raw->GetGammaRamp(0,&after);
        if(std::memcmp(&before,&after,sizeof(before)) || device.OutstandingStateLeases() || window.OutstandingTimerLeases())return 12;
        std::puts("actual gamma readback restored; device/timer leases empty");
    }
    return result;
}
