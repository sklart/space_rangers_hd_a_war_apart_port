"""Prepare the real MatrixGame interface declarations for native C++ only."""
import re
INPUTS={
 'MatrixGameDll.hpp':'f3ae60f5b1945b8ecae9e99749ee35ea3ce61a9112f4f7c8c90ffddf904147b0',
 'MatrixGame.h':'65b42d2d1f647cc328ec7d50ba983ad8e7fd60daccf82756796c3657117d018e',
}
SUBDIRS={name:'../../MatrixGame/src' for name in INPUTS}


def patch_engine_api(files):
    h=files['MatrixGameDll.hpp'].decode()
    h=h.replace('#include <windows.h>', '''#include <cstdint>
// Borrowed native caller contexts; this header implements no Windows functions.
using HINSTANCE=void*;
using HWND=void*;''')
    h=h.replace('long FDirect3D;','void* FDirect3D;').replace('long FD3DDevice;','void* FD3DDevice;')
    h=h.replace('SRobotsSettings settings;', 'SRobotsSettings settings{};')
    h=re.sub(r'\bDWORD\b','std::uint32_t',h);h=re.sub(r'\bBYTE\b','std::uint8_t',h)
    h=h.replace('__stdcall','').replace('__cdecl','')
    h=h.replace('__declspec(dllexport)','').replace('__declspec(dllimport)','')
    files['MatrixGameDll.hpp']=('#ifndef MATRIXGAME_NATIVE_API_HPP\n#define MATRIXGAME_NATIVE_API_HPP\n'+h+'\n#endif\n').encode()
    game=files['MatrixGame.h'].decode()
    files['MatrixGame.h']=('#ifndef MATRIXGAME_NATIVE_GAME_HPP\n#define MATRIXGAME_NATIVE_GAME_HPP\n'+game+'\n#endif\n').encode()
