"""Port original DLL bootstrap/WndProc through typed native platform providers.

Not the full 3g.cpp TU. Win32 provider supplies actual calls; unknown backend
caps/memory are explicit Unsupported, never successful fake device operations.
"""
from matrixgame_vector_object_overrides import function
import re

def patch_platform_3g(files):
    source=files['3g.cpp'].decode() # pin checked by core prepare.
    init=function(source,'void L3GInitAsDLL(')
    body=init[init.index('{')+1:init.rindex('}')]
    body=re.sub(r'/\*.*?\*/','',body,flags=re.S)
    body=body[:body.index('#ifdef DO_SMART_COLOROPS')]
    begin=body.index('    g_D3D =')
    end=body.index('    ASSERT_DX(g_D3DD->GetDeviceCaps',begin)
    body=body[:begin]+'''    s.device = &device;
    s.window = &window;
    s.attachment=window.Attach(WindowProcedure);
    if(!s.attachment)throw std::logic_error("window returned no attachment");
'''+body[end:]
    body=body.replace('    L3GDeinit();','    Deinit();\n    auto& s=Current();')
    body=body.replace('g_HInst = hinst','s.module = hinst').replace('g_Wnd = hwnd','s.window_identity = window.Identity()')
    body=body.replace('g_WndExtern = true','s.external = true')
    body=body.replace('ASSERT_DX(g_D3DD->GetDeviceCaps(&g_D3DDCaps));','s.caps=device.ReadCaps();')
    body=body.replace('INITFLAG(g_Flags, GFLAG_GAMMA, g_D3DDCaps.Caps2 & D3DCAPS2_CANCALIBRATEGAMMA);',
        'if(s.caps.calibrate_gamma)s.flags |= 1u<<5;else s.flags &= ~(1u<<5);')
    body=body.replace('''    if (FLAG(g_Flags, GFLAG_GAMMA))
        g_D3DD->GetGammaRamp(0, &g_StoreRamp0);''','''    if(s.caps.calibrate_gamma)for(std::uint32_t i=0;i<std::min(2u,s.caps.swapchain_count);++i) {
        s.saved_gamma[i]=device.ReadGamma(i);s.gamma_saved[i]=true;
    }''')
    body=body.replace('g_AvailableTexMem = g_D3DD->GetAvailableTextureMem() / (1024 * 1024);',
        's.available_texture_mib=static_cast<int>(device.AvailableTextureBytes()/(1024*1024));')
    definition=source.index('bool processInput(UINT message, WPARAM wParam, LPARAM lParam)\n{')
    input_code=function(source[definition:],'bool processInput(')
    wnd=function(source,'LRESULT CALLBACK L3G_WndProc(HWND hWnd,')
    input_code=input_code.replace('UINT message, WPARAM wParam, LPARAM lParam',
        'std::uint32_t message,std::uintptr_t wParam,std::intptr_t lParam')
    wnd=wnd.replace('LRESULT CALLBACK L3G_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)',
        'std::intptr_t WindowProcedure(Window& window,std::uint32_t message,std::uintptr_t wParam,std::intptr_t lParam)')
    wnd=wnd.replace('ShowWindow(g_Wnd, SW_MAXIMIZE);','window.Show(Visibility::Maximized);')
    wnd=wnd.replace('ShowWindow(g_Wnd, SW_RESTORE);','window.Show(Visibility::Restored);')
    wnd=wnd.replace('ShowWindow(g_Wnd, SW_MINIMIZE);','window.Show(Visibility::Minimized);')
    wnd=wnd.replace('ClipCursor(NULL);','window.ReleaseCursor();')
    wnd=wnd.replace('DefWindowProc(hWnd, message, wParam, lParam)','window.DefaultMessage(message,wParam,lParam)')
    wnd=wnd.replace('g_Flags','Current().flags')
    constants={'WM_PAINT':0x000f,'WM_ACTIVATEAPP':0x001c,'WM_SETCURSOR':0x0020,'WM_DESTROY':2,'WM_CLOSE':0x10,'WM_QUIT':0x12,
        'WM_MOUSEWHEEL':0x20a,'WM_MOUSEMOVE':0x200,'WM_LBUTTONDOWN':0x201,'WM_LBUTTONDBLCLK':0x203,
        'WM_RBUTTONDOWN':0x204,'WM_RBUTTONDBLCLK':0x206,'WM_MBUTTONDOWN':0x207,
        'WM_LBUTTONUP':0x202,'WM_RBUTTONUP':0x205,'WM_MBUTTONUP':0x208,
        'WM_KEYDOWN':0x100,'WM_SYSKEYDOWN':0x104,'WM_KEYUP':0x101,'WM_SYSKEYUP':0x105,
        'VK_LBUTTON':1,'VK_RBUTTON':2,'VK_MBUTTON':4,'GFLAG_KEEPALIVE':1<<9,'GFLAG_APPACTIVE':1,'GFLAG_FULLSCREEN':1<<3,'GFLAG_APPCLOSE':1<<2}
    prologue='\n'.join(f'constexpr std::uint32_t {name}={value};' for name,value in constants.items())
    files['3GPlatformNative.cpp']=('''// Original 3g.cpp DLL/input/WndProc paths; GPL-2.0-or-later.
#include "platform_3g.hpp"
#include "Form.hpp"
#include "BaseDef.hpp"
#include <algorithm>
#ifdef DO_SMART_COLOROPS
#error Native DLL bootstrap requires explicit texture-stage support for DO_SMART_COLOROPS.
#endif
namespace srhd_awa::matrixgame::platform3g {
'''+prologue+'''
static std::uint16_t HIWORD(std::uintptr_t x){return std::uint16_t(x>>16);}
static std::uint16_t LOWORD(std::uintptr_t x){return std::uint16_t(x);}
void InitAsDLL(void* hinst,Window& window,Device& device) {
    try {
'''+body+'''    Current().ready=true;
    } catch(...) {Deinit();throw;}
}
'''+input_code+'\n'+wnd+'\n}\n').encode()
