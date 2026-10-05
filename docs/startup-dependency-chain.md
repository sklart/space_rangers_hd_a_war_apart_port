# Startup dependency chain

| Level | Source/functions | Dependencies | Switch blocker | Status |
|---|---|---|---|---|
| entry | `src/program.cpp`: `main` → `Rangers::ProgramMain` | full translated runtime | pulls all Win32/UI units | not entered |
| runtime | `runtime_support.hpp`, `runtime/*.hpp` | Delphi RTL, x87 helpers, Win32 imports | x87 asm, `windows.hpp` imports | subset compiled |
| game globals | `Rangers.cpp`: Forms, `GR_Main`, globals | UI, threads, audio, registry | Win32 events/window/registry/Steam | blocked after CrcUnit |
| filesystem | `EC_File.cpp` → `TPackCollectionEC` | logical handles, package collection, lock | original `EC_HsFile.cpp` HANDLE/loose-file implementation | Milestone 4: replace only collection backend with Package adapter |
| resource slice | `TFileEC::TryAcquireReadHandle/ReadBuffer/SetPointer` | portable adapter, `Package`, runtime strings | none on reached packaged path | PASS on host: `DATA/Asteroid/00.gai`, CRC/seek differential |
| GR_Main configuration | `GR_Main::LoadLanguageAndPackages` → `LoadSelectedModInstallBlocks` → `aPacket::LoadConfiguredPackages` | `EC_Buf`, `EC_BlockPar`, `EC_Str`, `EC_Mem`, real globals, install/language/mod config, multi-package collection | common game-root resolver backs loose files and `FileExists` | Milestone 6: PASS on host for synthetic selected-mod precedence and release Russian configuration with 18 ordered sources |
| platform runtime bootstrap | `startup_slice` → real `aPacket::InitializePackageCollection` → real install load | portable timing, lifecycle cleanup, logical window token | SDL2 window on Switch; headless token in CI | Milestone 7: host lifecycle/failure PASS; ARM64 NRO build PASS |
| SDL window | `runtime_platform::CreateMainWindow` | SDL2 video + gamecontroller runtime | physical event loop not yet validated | M7: compiled ARM64; hardware pending |
| renderer bootstrap | `GR_DXInit` and Direct3D/OKGF bridge | renderer device and presentation | Direct3D semantics have no reached portable backend | NEXT |
| audio | DirectSound path | audio device | intentionally deferred | DEFERRED |
| resource | `CrcUnit.cpp`: `ComputeCrc32` | portable Delphi helpers | none on compiled path | PASS: linked ARM64 |
| renderer | `GR_Main`, `EC_OKGF`, OKGF | window, `okgf.dll` ABI | game-facing adapter incomplete | portable OKGF is fully built, linked and fills a CPU framebuffer |
| main game | `Rangers::ProgramMain` | all prior layers | prior blockers | not entered |

The NRO runs the portable platform startup then the real GR_Main configuration slice before its disk-layout-compatible `EC_HsFile` resource self-test against `DATA/common.pkg`; hardware execution remains unverified. `Rangers::ProgramMain` and `GR_Main::InitializePlatformRuntimeAndMainWindow` are not entered.
