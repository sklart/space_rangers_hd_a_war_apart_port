# MatrixGame Switch plan

Observed `MatrixGame.dll` imports: `WINMM`, `d3d9`, `d3dx9_43`, `KERNEL32`, `USER32`, `GDI32`, `ADVAPI32`, `SHELL32`. It is loaded as a separate x86 Windows binary and cannot run on ARM64 Horizon.

Status: **UNRESOLVED / BLOCKED for final port**, not a reason to fake success. First future step is to document Rangers loader calls, entry ABI, callbacks and data structures against the release baseline; then source/reconstruct MatrixGame under a separate renderer/input/audio adapter. Milestone 1 may start without it.
