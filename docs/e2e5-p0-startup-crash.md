# E2E-5 P0 startup investigation

Input: `G:/pack.zip`, specifically `e2e.log` and
`crash_reports/01791961718_0100d1d027e96000.log`. The two older crash
reports in the archive were not used for this symbolization.

The log contains two starts at `325cd84b432a9e174081a14305640ed0f3a945c5`.
The first reaches `runtime/settings`, reports `FAIL ... copy-cfg`, returns from
`ProgramMain`, and incorrectly prints `BOOT COMPLETE`. The second reaches
`script host engine` and crashes before `script host engine ready`. They have
different immediate failure paths.

The matching ELF is
`SpaceRangersHDAWarApartE2E4Combined-325cd84.elf`, SHA-256
`B29EAA00B22B709251B9EA2A5950E628874287A7E5F790100D3C484A5A6C96C2`.
Its `readelf -n` Build ID is `0471c228444cfb10590beec12bcfe0b0afeb1482`,
matching the crash module prefix. `addr2line -a -f -C -i` resolves:

| Module offset | Function / location |
| --- | --- |
| `0xBFA048` | `fde_insert` / `add_fdes`, libgcc `unwind-dw2-fde.c:537,862` |
| `0xBFA0B4` | `size_of_encoded_value` / `add_fdes`, libgcc `unwind-pe.h:74`, `unwind-dw2-fde.c:852` |
| `0xBFA8B0` | `init_object` / `search_object`, libgcc `unwind-dw2-fde.c:925,1142` |
| `0xBFAB3C` | `_Unwind_Find_FDE`, libgcc `unwind-dw2-fde.c:1245` |
| `0xBF8330` | `uw_frame_state_for`, libgcc `unwind-dw2.c:1008` |
| `0xBF874C` | `uw_init_context_1`, libgcc `unwind-dw2.c:1343` |
| `0xBF8EB0` | `_Unwind_RaiseException`, libgcc `unwind.inc:94` |
| `0xB970D0` | `__cxa_throw` (no source line) |

The caller chain below `__cxa_throw` is
`std::__throw_system_error` → `std::thread::_M_start_thread` →
`e2e_threads::Create` (`e2e_threads.cpp:63`) → `CreateThreadThunk` →
`SystemImports::BeginThread` → `TThreadEC_Create` → `TScriptThread_Create` →
`InitializeScriptBuiltinsAndConstants` → `InitializeScriptEngine` →
`InitializeScriptHostRuntime` → `ProgramMain`. The ASCII register text
`Not enough space` belongs to this thread-start system error. It does not
identify RAM or SD capacity as the cause. The crashing instruction at
`0xBFA048` stores through the unwinder's FDE index; `objdump` confirms the
fault is during exception handling after thread start failed.

The CFG path previously relied on `FileExists` and then used
`copy_file(... overwrite_existing ...)`. The supplied log does not record
the SD source and target paths or `error_code`, so the exact filesystem
condition on that card cannot be recovered from it. The revised path resolves
existing user CFG names without case sensitivity, checks the real template,
reports file types, readability, destination directory, error values and
filesystem space, and copies only when the user CFG is absent. A missing or
unreadable template fails the entrypoint before script workers are started,
without creating a placeholder. The same check remains at the original
runtime/settings boundary.

On Switch, thread creation now calls `pthread_create` directly with an
explicit 256 KiB stack and checks its return code without throwing a C++
`std::system_error` through this startup path. If thread creation still fails,
the log reports its numeric error and startup fails. The console must retest
whether this removes the underlying thread-start failure.

The original host `ProgramMain` reached a presented Main Menu frame, logged
`BOOT READY`, accepted New Game, and opened GameSettings. Isolated CFG tests
covered missing template, creating from a real template, preserving user
settings, mixed-case names, invalid destination, unreadable source, and a
second process run. A full host launch with the template removed returned
code 5 before `ProgramMain` and logged the first `copy-cfg` failure followed
by `BOOT FAILED`; it did not create a CFG. The CFG test passed under
ASan/UBSan. These host checks do
not establish Switch runtime success; the new combined NRO requires a physical
retest with its matching ELF and hashes.
