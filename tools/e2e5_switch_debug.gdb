# Invoke with the matching combined ELF as the final gdb argument.
# Connect to a device explicitly with: target remote HOST:PORT
set pagination off
set print pretty on
set disassemble-next-line on

define e2e5-snapshot
  info registers
  bt
  thread apply all bt
  info files
end
document e2e5-snapshot
Capture registers, current and all-thread backtraces, and loaded section bases.
For a PIE NRO, subtract the runtime module base before addr2line lookup.
end
