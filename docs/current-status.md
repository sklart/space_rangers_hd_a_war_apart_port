# Current status — Milestone 4 EC_File package slice complete

Baseline SHA-256 is verified: `83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.

- FPC spike: BLOCKED — no `fpc` toolchain/Horizon target available locally; upstream supports Linux x86_64 and macOS ARM64, not Horizon.
- C++ runtime: PARTIAL PASS — `CrcUnit.cpp`, `System.cpp` and `SystemImports.cpp` compile and link into the NRO. The entry calls the real `SystemImports::Randomize`; its two timing imports map to libnx ticks.
- Resource loader: PASS locally — ARM64-safe disk/runtime split, recursive tree with active-offset cycle guard, case-insensitive lookup, logical handles, seek/read API and `ZL02` compressed payload decoder. Host tests validate tree hash, real payload CRC32, cycle fixtures and corrupt-input rejection. Payloads and folder entry arrays have explicit allocation limits before vector allocation.
- Milestone 4: **HOST PASS / ARM64 BUILD PASS / HARDWARE PENDING**. The unmodified translated `EC_File.cpp` reads and seeks `DATA/Asteroid/00.gai` through the ordered portable package adapter; synthetic coverage includes raw and `ZL02` reads, missing paths, logical handle reuse and the per-package 16-slot boundary.
- Milestone 3.1: **HOST PASS / ARM64 BUILD PASS / HARDWARE PENDING**. Asset-free CI runs self/indirect-cycle, normal recursion, repeated completed offset, cleanup-after-failure, corrupt package and synthetic `ZL02` fixtures. ASan/UBSan are unavailable in the local devkitPro MSYS environment (missing sanitizer runtimes), so ordinary host regressions are the recorded local evidence.
- Switch skeleton: PASS — `SpaceRangersHDAWarApart.nro` builds with SDL2/libnx.
- OKGF: PASS for compile/link — the complete portable library and SoftFloat build for ARM64 with JPEG/PNG/zlib; NRO calls `OKGR_Fill_WORD` on a CPU framebuffer. Hardware presentation remains unverified.
- MatrixGame: BLOCKED as documented separate Windows/D3D9 x86 DLL.

Hardware validation remains PENDING and is tracked separately. It does not block host/cross development of Milestone 4; no gameplay path is hardware-verified before a real Switch run.
