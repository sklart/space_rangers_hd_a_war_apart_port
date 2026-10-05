# Decision gate: C++20 + devkitA64 + libnx

Выбран C++ путь. Он использует стандартный установленный Switch toolchain и уже даёт ARM64 `.nro`; FPC не имеет локально доступного Horizon target/runtime.

| Criterion | FPC | C++20 |
|---|---|---|
| toolchain | BLOCKED: `fpc` absent; no Horizon support declared | PASS: `aarch64-none-elf-g++` + libnx/SDL2 |
| ARM64 | Linux/macOS only | native devkitA64 |
| portability knowledge | strongest reference | must be implemented |
| pointer/x87 risk | documented solutions | known, targeted tests required |
| maintainability | new RTL/runtime likely | standard Switch C++ tooling |

**Result:** C++20 + devkitA64 + libnx + SDL2. This does not authorize gameplay migration until C++ provenance and per-subsystem differential tests are established.
