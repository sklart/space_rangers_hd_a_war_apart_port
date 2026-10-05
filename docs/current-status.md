# Current status — Milestone 1

Baseline SHA-256 is verified: `83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.

- FPC spike: BLOCKED — no `fpc` toolchain/Horizon target available locally; upstream supports Linux x86_64 and macOS ARM64, not Horizon.
- C++ spike: PARTIAL PASS — a standalone core unit (`CrcUnit.cpp`) parses with devkitA64; full source is blocked by direct Win32 adapters, 32-bit ABI assumptions and x87 semantics.
- Switch skeleton: PASS — `SpaceRangersHDAWarApart.nro` builds with SDL2/libnx.
- OKGF: PARTIAL PASS — portable C sources `pixels.c`, `math/precision.c`, `copy.c` parse under devkitA64; complete build awaits cross JPEG/PNG linkage and framebuffer smoke test.
- MatrixGame: BLOCKED as documented separate Windows/D3D9 x86 DLL.

Last successful subsystem: SDL2/libnx NRO link. Current blocker: no game runtime is linked; replacing Windows runtime/renderer ABI must precede it. Stop here per milestone rule.
