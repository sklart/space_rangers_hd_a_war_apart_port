# Windows dependency audit

Импорты сняты `objdump -p` с эталонного `Rangers.exe` и поставляемых DLL.

| dependency | purpose | Windows implementation | FPC replacement | C++ state | Switch replacement | status |
|---|---|---|---|---|---|---|
| kernel32/user32/gdi32 | окна, input, memory, files | direct imports | SDL/native wrappers | `runtime/windows.hpp` | platform common + SDL2/libnx | planned |
| ole32/oleaut32/comctl32/shell32/advapi32/version | COM, dialogs, registry, shell | direct imports | native adapters | direct import declarations | remove/portable services | planned |
| winmm/dsound | timing/audio | direct imports | SDL audio | DirectSound unit | SDL2 audio | planned |
| AVIFIL32/xvidcore | AVI/video | direct imports/DLL | portability reference | Windows video code | deferred; intro non-blocking | blocked design |
| libogg/libvorbis/libvorbisfile | music | DLLs | native Vorbis | DLL ABI | static portable decoder | planned |
| zlib | compression | `ZLib.dll` | portable zlib | `ZLib.dll` | static zlib | planned |
| okgf.dll | software renderer | DLL | vendored OKGF | DLL ABI | portable OKGF | spike partial PASS |
| MatrixGame.dll | planetary battles | D3D9/D3DX9 + Win32 imports | wrappers only | external DLL path | separate port | BLOCKED future |
| Steam | achievements/overlay | inspect runtime calls later | wrappers | Achievements unit | clean disable only | planned |
