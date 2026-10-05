# Startup dependency chain

| Level | Source/functions | Dependencies | Switch blocker | Status |
|---|---|---|---|---|
| entry | `src/program.cpp`: `main` → `Rangers::ProgramMain` | full translated runtime | pulls all Win32/UI units | not entered |
| runtime | `runtime_support.hpp`, `runtime/*.hpp` | Delphi RTL, x87 helpers, Win32 imports | x87 asm, `windows.hpp` imports | subset compiled |
| game globals | `Rangers.cpp`: Forms, `GR_Main`, globals | UI, threads, audio, registry | Win32 events/window/registry/Steam | blocked after CrcUnit |
| filesystem | `EC_File.cpp` → `TPackCollectionEC` | logical handles, package collection, lock | original `EC_HsFile.cpp` HANDLE/loose-file implementation | Milestone 4: replace only collection backend with Package adapter |
| resource slice | `TFileEC::TryAcquireReadHandle/ReadBuffer/SetPointer` | portable adapter, `Package`, runtime strings | none on reached packaged path | PASS on host: `DATA/Asteroid/00.gai`, CRC/seek differential |
| next startup blocker | `GR_Main::LoadLanguageAndPackages` → `aPacket::LoadConfiguredPackages` | `EC_BlockPar`, install/language/mod config, multi-package collection | Win32 window/audio/registry are earlier in `InitializePlatformRuntime` | not entered; configuration + collection expansion required |
| resource | `CrcUnit.cpp`: `ComputeCrc32` | portable Delphi helpers | none on compiled path | PASS: linked ARM64 |
| renderer | `GR_Main`, `EC_OKGF`, OKGF | window, `okgf.dll` ABI | game-facing adapter incomplete | portable OKGF is fully built, linked and fills a CPU framebuffer |
| main game | `Rangers::ProgramMain` | all prior layers | prior blockers | not entered |

The NRO runs the disk-layout-compatible first stage of `EC_HsFile` against `DATA/common.pkg`; hardware execution remains unverified.
