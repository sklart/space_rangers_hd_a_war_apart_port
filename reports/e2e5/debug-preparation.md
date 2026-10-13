# E2E-5.1 ARM64 symbolization

Use the **matching ELF** whose SHA-256 is recorded beside the prepared NRO in
`e2e4-combined-build-report.json`. The NRO is a PIE image, so a runtime PC is
not an ELF address until the module load base is subtracted. Record the base
from the debugger's loaded-section view for that process; ASLR can change it
between launches.

1. Start `aarch64-none-elf-gdb -q -x tools/e2e5_switch_debug.gdb MATCHING.elf`.
2. When a device and gdbstub are available, use `target remote HOST:PORT`.
3. At a stop, run `e2e5-snapshot`. Save the registers, all-thread backtraces,
   module base, and exact ELF/NRO hashes together.
4. Resolve a PC with `aarch64-none-elf-addr2line -f -C -e MATCHING.elf
   0xELF_OFFSET`, where `ELF_OFFSET` accounts for the PIE load base.

This file prepares inspection only. No device or gdbstub connection was tested
during the offline stage.
