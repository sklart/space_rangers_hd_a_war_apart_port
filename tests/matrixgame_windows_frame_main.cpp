// SDK owner for the original Windows Init; the engine borrows both providers.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9tex.h>
#include "platform_win32.hpp"
#include "buffer_resources.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>
namespace {
IDirect3DDevice9* frame_device{};
std::filesystem::path output_directory;
unsigned frame_index{};
template<class T> struct ComOwner {T* p{};~ComOwner(){if(p)p->Release();}};
}
void CaptureOriginalMapFrame(){
    namespace p=srhd_awa::matrixgame::platform3g;
    ComOwner<IDirect3DSurface9> source,copy;
    p::CheckDeviceResult(frame_device->GetRenderTarget(0,&source.p),"frame GetRenderTarget");
    D3DSURFACE_DESC description{};p::CheckDeviceResult(source.p->GetDesc(&description),"frame GetDesc");
    D3DVIEWPORT9 viewport{};p::CheckDeviceResult(frame_device->GetViewport(&viewport),"frame viewport");
    ComOwner<IDirect3DSurface9> depth;
    p::CheckDeviceResult(frame_device->GetDepthStencilSurface(&depth.p),"frame depth target");
    D3DSURFACE_DESC depth_desc{};p::CheckDeviceResult(depth.p->GetDesc(&depth_desc),"frame depth descriptor");
    if(viewport.X || viewport.Y || viewport.Width!=800 || viewport.Height!=600 ||
       depth_desc.Width!=800 || depth_desc.Height!=600)throw std::runtime_error("frame viewport/depth extent mismatch");
    if(description.Width!=800 || description.Height!=600 ||
       (description.Format!=D3DFMT_A8R8G8B8 && description.Format!=D3DFMT_X8R8G8B8))throw std::runtime_error("frame target extent/format mismatch");
    p::CheckDeviceResult(frame_device->CreateOffscreenPlainSurface(description.Width,description.Height,description.Format,D3DPOOL_SYSTEMMEM,&copy.p,nullptr),"frame system surface");
    p::CheckDeviceResult(frame_device->GetRenderTargetData(source.p,copy.p),"actual framebuffer readback");
    D3DLOCKED_RECT pixels{};p::CheckDeviceResult(copy.p->LockRect(&pixels,nullptr,D3DLOCK_READONLY),"frame LockRect");
    std::set<DWORD> colors;
    for(UINT y=0;y<description.Height;++y){
        const auto* row=reinterpret_cast<const DWORD*>(static_cast<const char*>(pixels.pBits)+y*pixels.Pitch);
        for(UINT x=0;x<description.Width;++x)colors.insert(row[x]&0xffffff);
    }
    p::CheckDeviceResult(copy.p->UnlockRect(),"frame UnlockRect");
    if(colors.size()<64)throw std::runtime_error("original map framebuffer is empty/uniform");
    const auto name=output_directory/("map-frame-"+std::to_string(frame_index++)+".png");
    p::CheckDeviceResult(D3DXSaveSurfaceToFileW(name.c_str(),D3DXIFF_PNG,copy.p,nullptr,nullptr),"save original map snapshot");
    std::printf("actual map framebuffer readback: %ux%u distinct_rgb=%zu snapshot=%ls\n",description.Width,description.Height,colors.size(),name.c_str());
}
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
    p::Presentation requested{};requested.BackBufferWidth=640;requested.BackBufferHeight=480;
    requested.SwapEffect=1;requested.Windowed=true;requested.hDeviceWindow=window.Identity();
    p::D3D9Device borrowed(raw);
    bool refused=false;try{borrowed.ResetPresentation(requested);}catch(const p::Unsupported&){refused=true;}
    if(!refused)throw std::runtime_error("borrowed device accepted unowned reset");
    refused=false;try{window.ResizeOuter(0,100);}catch(const std::invalid_argument&){refused=true;}
    if(!refused)throw std::runtime_error("invalid window size accepted");
    auto reset_state=device.CaptureState();
    ComOwner<IDirect3DSurface9> retained;
    p::CheckDeviceResult(raw->CreateRenderTarget(32,32,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&retained.p,nullptr),"test retained DEFAULT target");
    const auto failure=device.ResetPresentation(requested);
    if(failure>=0)throw std::runtime_error("Reset accepted retained DEFAULT resource");
    retained.p->Release();retained.p=nullptr;
    p::CheckDeviceResult(device.ResetPresentation(requested),"actual Reset retry after DEFAULT release");
    p::CheckDeviceResult(reset_state->Restore(),"restore after actual Reset retry");reset_state.reset();
    std::puts("borrowed reset refusal, invalid size, actual DEFAULT-resource Reset failure/release/retry PASS");
    {
        namespace b=srhd_awa::matrixgame::buffers;
        b::Context context;b::Scope scope(context);
        auto* vertex=context.CreateVertex(48,D3DFVF_XYZ|D3DFVF_DIFFUSE);
        auto* index=context.CreateIndex(6,b::IndexWidth::U16);
        std::memset(vertex->Map(0,48),0,48);vertex->Unmap();
        auto* words=static_cast<std::uint16_t*>(index->Map(0,6));words[0]=0;words[1]=1;words[2]=2;index->Unmap();
        p::CheckDeviceResult(device.BindStagingVertex(0,vertex,0,16),"SDK staging vertex binding");
        p::CheckDeviceResult(device.BindStagingIndex(index),"SDK staging index binding");
        ComOwner<IDirect3DVertexBuffer9> gpu_vertex;ComOwner<IDirect3DIndexBuffer9> gpu_index;UINT offset{},stride{};
        p::CheckDeviceResult(raw->GetStreamSource(0,&gpu_vertex.p,&offset,&stride),"read SDK staging vertex");
        p::CheckDeviceResult(raw->GetIndices(&gpu_index.p),"read SDK staging index");
        if(!gpu_vertex.p || !gpu_index.p || stride!=16 || context.Vertex().buffer!=vertex || context.Index()!=index)
            throw std::runtime_error("SDK/CPU staging bindings differ");
        p::CheckDeviceResult(device.BindStagingVertex(0,nullptr,0,0),"SDK staging vertex unbind");
        p::CheckDeviceResult(device.BindStagingIndex(nullptr),"SDK staging index unbind");
        ComOwner<IDirect3DVertexBuffer9> unbound_vertex;ComOwner<IDirect3DIndexBuffer9> unbound_index;
        p::CheckDeviceResult(raw->GetStreamSource(0,&unbound_vertex.p,&offset,&stride),"read SDK vertex unbind");
        p::CheckDeviceResult(raw->GetIndices(&unbound_index.p),"read SDK index unbind");
        if(unbound_vertex.p || unbound_index.p || context.Vertex().buffer || context.Index())throw std::runtime_error("SDK/CPU staging unbind failed");
        vertex->Release();index->Release();device.ReleaseStagingUploads();
        if(context.AllocatedBytes())throw std::runtime_error("staging binding owner leak");
        std::puts("actual SDK/CPU staging bind/unbind and owner cleanup PASS");
    }
}
int RunOriginalWindowsGame(void*,void*,std::uintptr_t,std::uintptr_t,
    srhd_awa::matrixgame::platform3g::Window&,srhd_awa::matrixgame::platform3g::Device&);
int main(int argc,char** argv){
    if(argc!=2)return 2;
    output_directory=std::filesystem::path(argv[1]);
    std::setvbuf(stdout,nullptr,_IONBF,0);
    auto* handler=AddVectoredExceptionHandler(1,TraceEngineException);
    struct HandlerOwner{void* value;~HandlerOwner(){if(value)RemoveVectoredExceptionHandler(value);}} own_handler{handler};
    const auto module=GetModuleHandleW(nullptr);
    const auto hwnd=CreateWindowExW(0,L"STATIC",L"MatrixGame E2E-5.20",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,module,nullptr);
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
    frame_device=raw;
    namespace p=srhd_awa::matrixgame::platform3g;
    p::Win32Window window(hwnd);p::D3D9Device device(raw,p::D3D9Device::PresentationAccess::Exclusive);
    CheckVideoProviders(window,device,module,raw);
    RECT original_window{};GetWindowRect(hwnd,&original_window);
    D3DGAMMARAMP before{},after{};raw->GetGammaRamp(0,&before);
    int result=0;
    for(int entry=0;entry<2;++entry){
        std::printf("original Init entry=%d\n",entry);
        result=RunOriginalWindowsGame(module,hwnd,reinterpret_cast<std::uintptr_t>(api),reinterpret_cast<std::uintptr_t>(raw),window,device);
        raw->GetGammaRamp(0,&after);
        if(std::memcmp(&before,&after,sizeof(before)) || device.OutstandingStateLeases() || window.OutstandingTimerLeases())return 12;
        RECT restored{};GetWindowRect(hwnd,&restored);
        if(std::memcmp(&original_window,&restored,sizeof(restored)))return 14;
        ComOwner<IDirect3DSurface9> restored_target,restored_depth;
        p::CheckDeviceResult(raw->GetRenderTarget(0,&restored_target.p),"restored target");
        D3DSURFACE_DESC description{};p::CheckDeviceResult(restored_target.p->GetDesc(&description),"restored target descriptor");
        D3DVIEWPORT9 restored_view{};p::CheckDeviceResult(raw->GetViewport(&restored_view),"restored viewport");
        if(description.Width!=640 || description.Height!=480 || restored_view.Width!=640 || restored_view.Height!=480)return 15;
        if(raw->GetDepthStencilSurface(&restored_depth.p)!=D3DERR_NOTFOUND)return 16;
        std::puts("actual gamma/window readback restored; device/timer leases empty");
        if(result)return result;
    }
    return result;
}
