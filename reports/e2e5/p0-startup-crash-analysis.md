# E2E-5 P0 startup crash analysis

Input: `D:\Download\pack(2).zip`, session `e2e.log`, report
`01791961718_0100d1d027e96000.log`. The two reports with IDs
`01791730547` and `01791784421` were not used for this stack.

The first launch reaches `runtime/settings`, logs `FAIL ... copy-cfg`, then
incorrectly logs `BOOT COMPLETE`. The second reaches `script host engine` and
crashes before `script host engine ready`. They are separate failures.

Matching ELF: `SpaceRangersHDAWarApartE2E4Combined-325cd84.elf`, SHA-256
`B29EAA00B22B709251B9EA2A5950E628874287A7E5F790100D3C484A5A6C96C2`,
GNU Build ID `0471c228444cfb10590beec12bcfe0b0afeb1482`; the crash module ID
begins with the same 20 bytes.

| Offset | Symbol | Source |
| --- | --- | --- |
| `0xBFA048` | `fde_insert` in `add_fdes` | libgcc `unwind-dw2-fde.c:537/862` |
| `0xBFA0B4` | `size_of_encoded_value` in `add_fdes` | libgcc `unwind-pe.h:74`, `unwind-dw2-fde.c:852` |
| `0xBFA8B0` | `init_object` in `search_object` | libgcc `unwind-dw2-fde.c:925/1142` |
| `0xBFAB3C` | `_Unwind_Find_FDE` | libgcc `unwind-dw2-fde.c:1245` |
| `0xBF8330` | `uw_frame_state_for` | libgcc `unwind-dw2.c:1008` |
| `0xBF874C` | `uw_init_context_1` | libgcc `unwind-dw2.c:1343` |
| `0xBF8EB0` | `_Unwind_RaiseException` | libgcc `unwind.inc:94` |
| `0xB970D0` | `__cxa_throw` | line unavailable |

The remaining return addresses show
`std::__throw_system_error` → `std::thread::_M_start_thread` →
`e2e_threads::Create` (`e2e_threads.cpp:63`) → `CreateThreadThunk` →
`SystemImports::BeginThread` → `EC_Thread::TThreadEC_Create` →
`TScriptThread_Create` → `aScriptFun::InitializeScriptBuiltinsAndConstants`
(`aScriptFun.cpp:19163`) → `aScript::InitializeScriptEngine` →
`Globals::InitializeScriptHostRuntime` → `Rangers::ProgramMain`.

At `0xBFA048`, the ELF instruction is `str x20, [x22, x0, lsl #3]`.
Report registers `x22=0x434982a9a0`, `x0=0xCC` resolve the destination to
`0x434982b000`, exactly the report's abort address. Thus the Data Abort occurs
while libgcc is unwinding a thrown thread-creation exception. The register
ASCII `Not enough space` is that exception's error message. It does not by
itself prove exhausted RAM or SD capacity.

The original `std::thread` implementation enters libnx pthread startup, which
also sets a thread core mask. The Switch-only adapter now calls
`threadCreate/threadStart` directly with the original 128 KiB default stack
size and a normal thread priority. If creation still fails, it records the raw
libnx Result and exits with `BOOT FAILED`; physical Switch verification is
required to establish whether direct creation restores launch.

The first launch's exact filesystem error is absent from the old log. The old
CFG branch used a case-sensitive existence check followed by a copy with
`overwrite_existing`, without recording either resolved path or `error_code`.
The new branch resolves case variants, reuses a valid user file, and copies
only a real readable template with `copy_options::none`. Its pre-copy log will
identify any remaining SD-specific file or directory error.
