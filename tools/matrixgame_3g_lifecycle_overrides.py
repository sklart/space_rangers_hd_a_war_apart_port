"""Isolate the original DLL entry's CPU context prefix, never a fake device Init."""
INPUTS={'3g.cpp':'e02ae38d13e88a6b83d893f974cbfeebf936938e01c7f03097cd8be32a29605f'}
SUBDIRS={'3g.cpp':'../3G'}


def patch_3g_cpu_phase(files):
    source=files['3g.cpp'].decode()
    start=source.index('void L3GInitAsDLL(')
    start=source.index('    L3GDeinit();',start)
    end=source.index('    g_D3D =',start)
    prefix=source[start:end].replace('L3GDeinit();','L3GDeinitCpuPhase();')
    if 'g_HInst = hinst;' not in prefix or 'g_WndExtern = true;' not in prefix:
        raise ValueError('Pinned L3G DLL CPU prefix changed')
    # No Win32 window hook/device call is substituted. The next operation is
    # explicitly unsupported until the full platform/device boundary is ported.
    files['L3GCpuPhase.cpp']=('''// Original 3g.cpp prefix; GPL-2.0-or-later.
#include "init_runtime.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace { void* g_HInst{}; void* g_Wnd{}; bool g_WndExtern{}; }
void L3GInitAsDLLCpuPhase(void* hinst,void* hwnd) {
    if(g_WndExtern)throw std::logic_error("live borrowed 3G context already bound");
'''+prefix+'''}
void L3GDeinitCpuPhase() noexcept {
    // CPU prefix borrowed only these identities; it installed no WndProc and
    // created no D3D/deko device, window or gamma state to release.
    g_WndExtern=false;g_Wnd=nullptr;g_HInst=nullptr;
}
Cpu3GState InspectCpu3GState() noexcept {return {g_HInst,g_Wnd,g_WndExtern};}
}
''').encode()
