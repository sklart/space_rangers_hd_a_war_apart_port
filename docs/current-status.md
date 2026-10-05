# Current status — Milestone 2 in progress

Baseline SHA-256 is verified: `83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.

- FPC spike: BLOCKED — no `fpc` toolchain/Horizon target available locally; upstream supports Linux x86_64 and macOS ARM64, not Horizon.
- C++ runtime: PARTIAL PASS — `CrcUnit.cpp`, `System.cpp` and `SystemImports.cpp` compile and link into the NRO. The entry calls the real `SystemImports::Randomize`; its two timing imports map to libnx ticks.
- Resource loader: PARTIAL PASS — a fixed-width port of the initial `TPackFileEC::Open` / `THsFolderEC::Load` disk path validates `DATA/common.pkg` root metadata. Desktop regression passes against the supplied release data.
- Switch skeleton: PASS — `SpaceRangersHDAWarApart.nro` builds with SDL2/libnx.
- OKGF: PASS for compile/link — the complete portable library and SoftFloat build for ARM64 with JPEG/PNG/zlib; NRO calls `OKGR_Fill_WORD` on a CPU framebuffer. Hardware presentation remains unverified.
- MatrixGame: BLOCKED as documented separate Windows/D3D9 x86 DLL.

Last successful subsystem: real C++ `SystemImports::Randomize`, `CrcUnit`, first-stage `EC_HsFile` package-root parsing, and linked portable OKGF framebuffer fill. Current blocker: `TPackFileEC` calls Win32 HANDLE functions and embeds host pointers in in-memory records; this blocks the full package collection and `Rangers::ProgramMain`.
