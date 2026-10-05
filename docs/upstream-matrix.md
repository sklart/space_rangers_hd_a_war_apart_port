# Матрица upstream

Все hashes ниже получены локально 2026-10-05. `windows/` — read-only и в Git не включён.

| implementation | source revision | game revision | compiler | CPU | pointer width | OS | renderer | platform layer | external DLLs | portability status |
|---|---|---|---|---|---|---|---|---|---|---|
| decomp-release | `730bdf6` (`build-2025-10-13`) | Steam/GOG 2025-10-13, `Rangers.exe` 2.1.2500 | Delphi reconstruction | x86 | 32 | Windows | original OKGF DLL | Win32 | yes | baseline only |
| decomp-current | `5f491a8` main | newer prerelease lineage | Delphi reconstruction | x86 | 32 | Windows | original interfaces | Win32 | yes | analysis only |
| FPC | `5f491a8`; generated from decomp `7342a10` | README: prerelease 2026-08-11 | patched FPC | x86_64/aarch64 | 64 | Linux/macOS | portable OKGF | SDL/native | native `.so` | portability reference, not gameplay baseline |
| C++ | pinned submodule `57fa689` | README: 2.1.2500; exact decomp commit not embedded | C++20/Zig | x86 | 32 | Windows/Wine | original DLL ABI | Win32 adapters | yes | candidate; `CrcUnit.cpp` linked on Switch |
| OKGF | current clone | SRHD mode selectable | C11/CMake | portable | n/a | portable | source | C platform file | JPEG/PNG/zlib | selected renderer candidate |
| MatrixGame | Windows `MatrixGame.dll` only | release installation | unknown | x86 | 32 | Windows | D3D9 | Win32 | D3D9/D3DX9 | separate future subsystem |

FPC provenance подтверждён его README и commit `7342a10dc1a0dcaa242ea4bc8c33e29c0eb6bdc0` (2026-09-22). Для C++ подтверждены game version и связь с decomp, но не точный исходный commit: это **UNKNOWN**, не допустимое основание переносить gameplay.
