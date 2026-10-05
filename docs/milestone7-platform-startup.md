# Milestone 7 — portable platform startup

Release oracle: `SpaceRangersHD_decomp` build `2025-10-13`, commit `730bdf6`.
The implementation keeps the translated `GR_Main.cpp` as the game source; it
does not copy or call its monolithic Windows initializer.

## Release initializer decomposition

| Release operation | Category | Purpose/order | Portable M7 result |
|---|---|---|---|
| `GetSystemDirectoryW`, `LoadLibraryW(d3d9)`, `GetProcAddress` | E/F | Direct3D dynamic loading | not executed; renderer is next boundary |
| `LoadLibraryW(dsound)`, DirectSound exports | D/F | audio backend | deferred |
| `CheckPlatformModules` | E/F | Windows module checks | not executed |
| `CoInitialize` | B/F | COM platform setup | not applicable; no fake COM |
| `Ex_OKGF_DXVersion` | C/F | renderer capability logging | deferred with renderer; not hardcoded |
| `RegisterWindowMessage` | E/F | Win32 message integration | not needed before event loop |
| `CopyFile(#ship_c.dbf, #ship.dbf)` | A/F | creates a working copy when its optional source exists | baseline source is absent; release ignores failure; deferred until a downstream consumer is reached, never writes game assets |
| startup logging | A/B | diagnostics before config | portable `SessionLog` points at `sdmc:/switch/space-rangers-hd-a-war-apart/port.log` |
| `QueryPerformanceFrequency` | B | timing capability | portable 1 GHz nanosecond tick frequency; existing runtime time imports use libnx ticks |
| class registration + `CreateWindowExW` | B/E | native presentation host | SDL2 window on Switch; headless token on host; no pointer truncation |
| `InitializePackageCollection` | A | precedes install configuration | real `aPacket::InitializePackageCollection()` |
| timer + `Forms::Application->Handle` | B/E | Windows event/VCL plumbing | deferred; no VCL emulator |
| load `INSTALL.TXT` | A | base configuration | real `TBlockParEC` load through `EC_File` |
| `TQuestMessages.Create` | A | post-install game state | real `MessageText.cpp` linked and instantiated |

The retained semantic sequence after portable services is package collection,
logical window, install configuration, then language/package configuration and
quest messages—the same ordering of game-owned operations as release.

## Portable policy

`runtime_platform` uses SDL2 only on Switch and owns the native pointer. It
publishes a monotonically allocated nonzero token to `GR_Main::MainWindowHandle`;
the reached M7 functions only require its validity. The token is cleared on
shutdown. Runtime writes are restricted to `sdmc:/switch/space-rangers-hd-a-war-apart/`:
`game/` is read-only user assets; future `save/`, `config/`, `logs/`, and
`runtime/` files belong under that writable root.

`test_platform_startup.cpp` validates a headless init/config/resource/shutdown
cycle, a second initialization after cleanup, and clean failures for missing
install, missing language, and invalid package files. It also confirms that
`TQuestMessages` is real, not a fake object.

The precise next blocker is `GR_Main::GR_DXInit` and its renderer device/
presentation dependency. M7 does not enter it, DirectSound, a message loop or
`Rangers::ProgramMain`.
