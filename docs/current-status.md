# Current status — Milestone 3 complete locally

Baseline SHA-256 is verified: `83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.

- FPC spike: BLOCKED — no `fpc` toolchain/Horizon target available locally; upstream supports Linux x86_64 and macOS ARM64, not Horizon.
- C++ runtime: PARTIAL PASS — `CrcUnit.cpp`, `System.cpp` and `SystemImports.cpp` compile and link into the NRO. The entry calls the real `SystemImports::Randomize`; its two timing imports map to libnx ticks.
- Resource loader: PASS locally — ARM64-safe disk/runtime split, recursive tree, case-insensitive lookup, logical handles, seek/read API and `ZL02` compressed payload decoder. Host tests validate tree hash, real payload CRC32 and corrupt input rejection.
- Switch skeleton: PASS — `SpaceRangersHDAWarApart.nro` builds with SDL2/libnx.
- OKGF: PASS for compile/link — the complete portable library and SoftFloat build for ARM64 with JPEG/PNG/zlib; NRO calls `OKGR_Fill_WORD` on a CPU framebuffer. Hardware presentation remains unverified.
- MatrixGame: BLOCKED as documented separate Windows/D3D9 x86 DLL.

Hardware status remains PENDING: NRO has not been run on a physical Switch. `GR_Main` remains deliberately unconnected until that package gate is manually verified.
