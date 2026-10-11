# Pointer-width audit

| location | original type | semantic purpose | FPC solution | C++ Switch solution | status |
|---|---|---|---|---|---|
| `TPackEntryEC` | serialized 158-byte entry plus `ChildFolder*` | package directory record | retain disk layout | parse disk fields explicitly; no host pointer deserialization | PASS for root record |
| `TPackOpenSlotEC` | `uint32_t FileHandle`, buffers | runtime handle/buffers | platform wrapper | replace Win32 handle before collection port | TODO |
| `aGalaxy.cpp` object-id casts | pointer stored in `uint32_t` | tagged IDs / load transition | reference needed per use | classify ID cell vs pointer; no global widening | TODO |
| `aSaveLoad.cpp:459,474` memory snapshot galaxy token | actual `TGalaxy*` truncated to `uint32_t`, then reconstructed and dereferenced | transient in-process snapshot, never serialized; the 32-bit additive mask is only an address disguise | no FPC comparison needed to establish width error | E2E source copy stores the masked address in `uintptr_t`; keeps the old exported `uint32_t` symbol unused for ABI compatibility | ARM64 LINK PASS; snapshot runtime pending |
| `aGalaxy.cpp:3451-3555,3727,3745` protected-state XOR | actual data and field addresses narrowed to `uint32_t`; object header and excluded pointer fields assumed four bytes | in-memory galaxy obfuscation during save/snapshot and restore | compare object layout and protected fields before changing | E2E-4 disposable override uses native pointer ranges and widths; synthetic round-trip and ARM64 layout/object compilation pass, real galaxy round-trip pending | PARTIAL |
| `aGalaxy.cpp:1207-1400` load-transition ID cells | serialized 32-bit object IDs temporarily held in list/pointer slots until `IdTo*` resolution | on-disk IDs, not process addresses at these call sites | preserve ID semantics | do not widen disk IDs; inspect each transition for lifetime/order errors | CLASSIFIED for listed transition sites; runtime pending |
| `aScript.cpp:547,1462,1496,2151-2161,3044-3127,4052-4217` script values | actual object pointers narrowed for `lvDword` script variables and bindings | live script object references | FPC reference needed for script value ABI | requires pointer-safe script value representation; cannot reinterpret as 32-bit serialized IDs wholesale | OPEN |
| `aScript.cpp:4466-4507` UI cache order | actual `TBlockParEC*` narrowed to 32-bit comparison key | in-memory sort/search | none needed | compare `uintptr_t` keys or pointers with a defined strict order | OPEN |
| `runtime/windows.hpp` ordinal import | `uintptr_t` | DLL ordinal selector | native loader | remove DLL path on Switch | TODO |
| `CrcUnit` | `void*`, `int32_t` byte count | transient bytes | portable | unchanged; never serialized | PASS |

First validated release package has `entry_record_size=158`; direct `sizeof`/layout reuse is prohibited for package structs containing pointers.

## E2E-3 targeted reachability (2026-10-11)

- `TGalaxy::XorProtectedState` is reached by `aSaveLoad` memory-snapshot obfuscation/restoration and by galaxy cleanup if protected state remains. Its `uint32_t` pointer casts and four-byte VMT/field skips are confirmed unsafe for ARM64. This is a save/snapshot blocker, not a reason to change serialized 32-bit IDs. A correct fix needs a protected-field layout test and matching XOR round-trip across the actual 64-bit object types.
- `aScript.cpp` stores actual star/ship/planet/script-item pointers and function addresses in `lvDword`/32-bit signatures. `fGameSettings::TThreadCreateNewGame_Execute` calls `aScript::RunGlobalScriptsForContext` at the end of generation, so script pointer truncation is potentially on the new-game path. An address/token map or pointer-safe script value needs a focused script-binding regression before mutation.
- The `aSaveLoad` snapshot galaxy token uses the existing E2E source-copy `uintptr_t` fix. This protects one transient pointer but does not establish a snapshot or disk save round-trip.
- ARM64 `long double` has a 113-bit mantissa and is not x87 Extended. No evidence yet establishes compatibility of every raw numeric field in saves; inspect concrete serializer calls before changing the format.
