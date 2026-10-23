// Actual SDK creation is isolated from the portable CPU D3DX math names.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9core.h>
#include "platform_win32.hpp"
#include "console_backend.hpp"
#include "debug_font_backend.hpp"
#include <cstdio>
#include <cassert>
#include <array>
#include <cstring>
#include <cwchar>
namespace p=srhd_awa::matrixgame::platform3g;
namespace console_keys=srhd_awa::matrixgame::console;
static_assert(console_keys::Back==VK_BACK && console_keys::Delete==VK_DELETE && console_keys::Left==VK_LEFT);
static_assert(console_keys::Right==VK_RIGHT && console_keys::Home==VK_HOME && console_keys::End==VK_END);
static_assert(console_keys::Return==VK_RETURN && console_keys::Escape==VK_ESCAPE && console_keys::Shift==VK_SHIFT);
static_assert(static_cast<unsigned>(p::PixelFormat::Argb8)==D3DFMT_A8R8G8B8 && static_cast<unsigned>(p::PixelFormat::Xrgb8)==D3DFMT_X8R8G8B8);
static_assert(static_cast<unsigned>(p::PixelFormat::Rgb565)==D3DFMT_R5G6B5 && static_cast<unsigned>(p::PixelFormat::Depth16)==D3DFMT_D16);
static_assert(static_cast<unsigned>(p::PixelFormat::Depth24Stencil8)==D3DFMT_D24S8 && static_cast<unsigned>(p::PixelFormat::Depth24)==D3DFMT_D24X8);
static_assert((DT_CALCRECT|DT_SINGLELINE)==0x420u);
void VerifyPortableDebugFontText(ID3DXFont*);
void VerifyFormattedBitmapDevice();
static_assert(D3DFMT_DXT1==0x31545844u && D3DPOOL_MANAGED==1);
static void VerifyDebugFont(p::Device& device,IDirect3DDevice9* raw) {
    namespace font=srhd_awa::matrixgame::debug_font;
    const auto leases=device.OutstandingStateLeases();
    for(int cycle=0;cycle<2;++cycle) {
        ID3DXFont* value=nullptr;
        p::CheckDeviceResult(font::CreateFor(device,&value),"original D3DX font request");
        assert(value);
        struct Owner {ID3DXFont* value;~Owner(){font::Release(value);}} owner{value};
        IDirect3DDevice9* actual=nullptr;
        assert(SUCCEEDED(value->GetDevice(&actual)) && actual==raw);actual->Release();
        D3DXFONT_DESCW desc{};assert(SUCCEEDED(value->GetDescW(&desc)));
        assert(desc.Height==25 && desc.Width==0 && desc.Weight==FW_NORMAL && desc.MipLevels==1 && !desc.Italic);
        assert(desc.CharSet==DEFAULT_CHARSET && desc.OutputPrecision==OUT_DEFAULT_PRECIS && desc.Quality==PROOF_QUALITY);
        assert(desc.PitchAndFamily==DEFAULT_PITCH && std::wcscmp(desc.FaceName,L"MS Sans Serif")==0);
        bool duplicate=false;
        try{font::CreateFor(device,&value);}catch(const std::invalid_argument&){duplicate=true;}
        assert(duplicate && value==owner.value);
        // Real glyph measurement, not framebuffer/pixel or rendered-text proof.
        font::TextRect bounds{};
        const int height=font::DrawTextWide(value,L"MatrixGame",-1,bounds,DT_CALCRECT|DT_SINGLELINE,0xffffffffu);
        assert(height>0 && bounds.right>bounds.left && bounds.bottom>bounds.top);
        VerifyPortableDebugFontText(value);
        p::CheckDeviceResult(font::Lost(value),"font lost-device notification");
        p::CheckDeviceResult(font::Reset(value),"font reset-device notification");
    }
    assert(device.OutstandingStateLeases()==leases);
    std::puts("actual D3DX font request/device identity/descriptor, glyph measurement, lost/reset notifications and Release/recreate PASS; no device Reset or rendered pixels claimed");
}
int RunWindowsBootstrap(int argc,char**argv,int(*run)(int,char**,void*,p::Window&,p::Device&)) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::puts("Windows SDK: creating borrowed host window/device");
    const auto module=GetModuleHandleW(nullptr);
    const auto hwnd=CreateWindowExW(0,L"STATIC",L"MatrixGame bootstrap",WS_POPUP,0,0,16,16,nullptr,nullptr,module,nullptr);
    if(!hwnd)return 77;
    struct WindowOwner {HWND h;~WindowOwner(){DestroyWindow(h);}} own_window{hwnd};
    auto* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api)return 77;
    struct ApiOwner {IDirect3D9* p;~ApiOwner(){p->Release();}} own_api{api};
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=hwnd;
    pp.BackBufferWidth=16;pp.BackBufferHeight=16;
    IDirect3DDevice9* raw=nullptr;
    const auto hr=api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_NULLREF,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&raw);
    if(FAILED(hr)){std::fprintf(stderr,"D3D9 NULLREF unavailable hr=%08lx\n",hr);return 77;}
    std::puts("Windows SDK: actual D3D9 NULLREF created");
    struct DeviceOwner {IDirect3DDevice9*p;~DeviceOwner(){p->Release();}} own_device{raw};
    IDirect3DTexture9* texture=nullptr;
    if(FAILED(raw->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr)))return 77;
    struct TextureOwner {IDirect3DTexture9*p;~TextureOwner(){p->Release();}} own_texture{texture};
    if(FAILED(raw->SetTexture(0,texture)))return 77; // Real borrowed binding must survive native ClearTexture/Deinit.
    p::Win32Window window(hwnd);p::D3D9Device device(raw);
    VerifyDebugFont(device,raw);
    {
        p::InitAsDLL(module,window,device);
        struct Scope {~Scope(){p::Deinit();}} scope;
        auto state=device.CaptureState();
        VerifyFormattedBitmapDevice();
        p::CheckDeviceResult(state->Restore(),"formatted texture state restoration");
    }
    assert(!p::Current().ready && device.OutstandingStateLeases()==0);
    IDirect3DVertexBuffer9* stream0=nullptr;
    assert(SUCCEEDED(raw->CreateVertexBuffer(64,0,D3DFVF_XYZ,D3DPOOL_MANAGED,&stream0,nullptr)));
    struct StreamOwner {IDirect3DVertexBuffer9*p;~StreamOwner(){p->Release();}} own_stream{stream0};
    assert(SUCCEEDED(raw->SetStreamSource(0,stream0,4,12)));
    D3DCAPS9 direct_caps{};assert(SUCCEEDED(raw->GetDeviceCaps(&direct_caps)));
    const auto actual_caps=device.ReadCaps();
    assert(actual_caps.max_simultaneous_textures==direct_caps.MaxSimultaneousTextures);
    assert(actual_caps.max_texture_stages==direct_caps.MaxTextureBlendStages);
    bool uncaptured_layout=false;try{device.SetFixedVertexLayout(D3DFVF_XYZ);}catch(const p::Unsupported&){uncaptured_layout=true;}
    assert(uncaptured_layout);
    constexpr D3DTEXTURESTAGESTATETYPE stage_keys[]{D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,
        D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_TEXCOORDINDEX,D3DTSS_RESULTARG,D3DTSS_TEXTURETRANSFORMFLAGS};
    constexpr DWORD changed[]{D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_CURRENT,D3DTOP_SELECTARG1,
        D3DTA_DIFFUSE,D3DTA_CURRENT,1,D3DTA_CURRENT,D3DTTFF_COUNT2};
    DWORD before_stage[std::size(stage_keys)]{},before_fvf{};D3DMATRIX before_matrix{},before_view{},before_projection{};
    assert(SUCCEEDED(raw->GetTransform(D3DTS_VIEW,&before_view)));
    assert(SUCCEEDED(raw->GetTransform(D3DTS_PROJECTION,&before_projection)));
    assert(SUCCEEDED(raw->GetFVF(&before_fvf)) && SUCCEEDED(raw->GetTransform(D3DTS_TEXTURE2,&before_matrix)));
    for(unsigned key=0;key<std::size(stage_keys);++key)assert(SUCCEEDED(raw->GetTextureStageState(2,stage_keys[key],&before_stage[key])));
    IDirect3DVertexDeclaration9* before_declaration=nullptr;
    assert(SUCCEEDED(raw->GetVertexDeclaration(&before_declaration)));
    struct DeclarationOwner {IDirect3DVertexDeclaration9*p;~DeclarationOwner(){if(p)p->Release();}} own_declaration{before_declaration};
    for(int cycle=0;cycle<2;++cycle) {
        auto state=device.CaptureState();assert(device.OutstandingStateLeases()==1);
        auto changed_view=before_view,changed_projection=before_projection;
        changed_view._41=13.0f+cycle;changed_projection._22=7.0f+cycle;
        assert(SUCCEEDED(raw->SetTransform(D3DTS_VIEW,&changed_view)));
        assert(SUCCEEDED(raw->SetTransform(D3DTS_PROJECTION,&changed_projection)));
        assert(device.SetFixedVertexLayout(D3DFVF_XYZ)==p::DeviceSuccess);
        const float triangle[9]{0,0,0,1,0,0,0,1,0};
        assert(SUCCEEDED(raw->BeginScene()));
        const auto draw=raw->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,1,triangle,12);
        assert(SUCCEEDED(raw->EndScene()) && SUCCEEDED(draw));
        IDirect3DVertexBuffer9* after_draw=nullptr;UINT offset{},stride{};
        assert(SUCCEEDED(raw->GetStreamSource(0,&after_draw,&offset,&stride)));
        const bool cleared=after_draw==nullptr;if(after_draw)after_draw->Release();
        assert(cleared); // Documented DrawPrimitiveUP side effect, observed directly.
        assert(device.BindBorrowedTexture(0,nullptr)==p::DeviceSuccess);
        for(unsigned key=0;key<std::size(stage_keys);++key)
            assert(device.SetTextureStageState(2,static_cast<p::TextureStageRegister>(stage_keys[key]),changed[key])==p::DeviceSuccess);
        p::WorldMatrix transform{1,0,0,0,0,1,0,0,0,0,1,0,0.25f,0.5f,0,1};
        assert(device.SetTextureMatrix(2,transform)==p::DeviceSuccess);
        D3DMATRIX check{};DWORD fvf{};
        assert(SUCCEEDED(raw->GetTransform(D3DTS_TEXTURE2,&check)) && std::memcmp(&check,transform.data(),sizeof(check))==0);
        assert(SUCCEEDED(raw->GetFVF(&fvf)) && fvf==D3DFVF_XYZ);
        assert(state->Restore()==p::DeviceSuccess);state.reset();assert(device.OutstandingStateLeases()==0);
        IDirect3DVertexBuffer9* restored_stream=nullptr;
        assert(SUCCEEDED(raw->GetStreamSource(0,&restored_stream,&offset,&stride)));
        const bool stream_restored=restored_stream==stream0 && offset==4 && stride==12;
        if(restored_stream)restored_stream->Release();
        assert(stream_restored);
        assert(SUCCEEDED(raw->GetFVF(&fvf)) && fvf==before_fvf);
        assert(SUCCEEDED(raw->GetTransform(D3DTS_TEXTURE2,&check)) && std::memcmp(&check,&before_matrix,sizeof(check))==0);
        assert(SUCCEEDED(raw->GetTransform(D3DTS_VIEW,&check)) && std::memcmp(&check,&before_view,sizeof(check))==0);
        assert(SUCCEEDED(raw->GetTransform(D3DTS_PROJECTION,&check)) && std::memcmp(&check,&before_projection,sizeof(check))==0);
        for(unsigned key=0;key<std::size(stage_keys);++key) {
            DWORD value{};assert(SUCCEEDED(raw->GetTextureStageState(2,stage_keys[key],&value)) && value==before_stage[key]);
        }
        IDirect3DVertexDeclaration9* declaration=nullptr;assert(SUCCEEDED(raw->GetVertexDeclaration(&declaration)));
        assert(declaration==before_declaration);if(declaration)declaration->Release();
        IDirect3DBaseTexture9* binding=nullptr;assert(SUCCEEDED(raw->GetTexture(0,&binding)));
        assert(binding==texture);if(binding)binding->Release();
    }
    std::puts("real SDK renderer state lease: FVF/declaration, nine stage registers, texture/view/projection matrices, bindings and stream0 after actual DrawPrimitiveUP restored over two entries; no rendered pixel proof");
    D3DDEVICE_CREATION_PARAMETERS direct_creation{};
    assert(SUCCEEDED(raw->GetCreationParameters(&direct_creation)));
    const auto creation=device.ReadCreationParameters();
    const auto legacy=device.ReadLegacyHandles();
    assert(legacy.api==reinterpret_cast<std::uintptr_t>(api) && legacy.device==reinterpret_cast<std::uintptr_t>(raw));
    std::printf("direct SDK creation: adapter=%u type=%u flags=%lu focus=%p expected=%p\n",direct_creation.AdapterOrdinal,direct_creation.DeviceType,direct_creation.BehaviorFlags,direct_creation.hFocusWindow,hwnd);
    assert(creation.AdapterOrdinal==direct_creation.AdapterOrdinal && creation.DeviceType==direct_creation.DeviceType && creation.BehaviorFlags==direct_creation.BehaviorFlags && creation.FocusWindow==direct_creation.hFocusWindow);
    D3DDISPLAYMODE direct_mode{};assert(SUCCEEDED(api->GetAdapterDisplayMode(creation.AdapterOrdinal,&direct_mode)));
    const auto mode=device.ReadAdapterDisplayMode(creation.AdapterOrdinal);
    assert(mode.Width==direct_mode.Width && mode.Height==direct_mode.Height && mode.RefreshRate==direct_mode.RefreshRate && static_cast<std::uint32_t>(mode.Format)==direct_mode.Format);
    RECT direct_client{},direct_outer{};
    assert(GetClientRect(hwnd,&direct_client) && GetWindowRect(hwnd,&direct_outer));
    assert(window.ReadClientRect()==(p::WindowRect{direct_client.left,direct_client.top,direct_client.right,direct_client.bottom}));
    assert(window.ReadWindowRect()==(p::WindowRect{direct_outer.left,direct_outer.top,direct_outer.right,direct_outer.bottom}));
    const auto raw_multi=api->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_A8R8G8B8,TRUE,D3DMULTISAMPLE_NONE,nullptr);
    assert(device.CheckMultisample(0,1,p::PixelFormat::Argb8,true,0)==static_cast<p::DeviceResult>(raw_multi));
    bool invalid_adapter=false;
    try{device.ReadAdapterDisplayMode(0xffffffffu);}catch(const p::DeviceError& e){invalid_adapter=e.result==static_cast<p::DeviceResult>(D3DERR_INVALIDCALL);}
    assert(invalid_adapter);
    const auto saved_cursor=GetCursor();const auto arrow=LoadCursorA(nullptr,IDC_ARROW);
    if(!arrow)return 77;
    SetCursor(arrow);
    struct CursorOwner {HCURSOR old;~CursorOwner(){SetCursor(old);}} restore_cursor{saved_cursor};
    const auto result=run(argc,argv,module,window,device);
    assert(window.OutstandingTimerLeases()==0);
    return result;
}
