# Pointer-width audit

| location | original type | semantic purpose | FPC solution | C++ Switch solution | status |
|---|---|---|---|---|---|
| `TPackEntryEC` | serialized 158-byte entry plus `ChildFolder*` | package directory record | retain disk layout | parse disk fields explicitly; no host pointer deserialization | PASS for root record |
| `TPackOpenSlotEC` | `uint32_t FileHandle`, buffers | runtime handle/buffers | platform wrapper | replace Win32 handle before collection port | TODO |
| `aGalaxy.cpp` object-id casts | pointer stored in `uint32_t` | tagged IDs / load transition | reference needed per use | classify ID cell vs pointer; no global widening | TODO |
| `aSaveLoad.cpp:459,474` memory snapshot galaxy token | actual `TGalaxy*` truncated to `uint32_t`, then reconstructed and dereferenced | transient in-process snapshot, never serialized; the 32-bit additive mask is only an address disguise | no FPC comparison needed to establish width error | E2E source copy stores the masked address in `uintptr_t`; keeps the old exported `uint32_t` symbol unused for ABI compatibility | ARM64 LINK PASS; snapshot runtime pending |
| `aGalaxy.cpp:3451-3555,3727,3745` protected-state XOR | actual data and field addresses narrowed to `uint32_t`; object header and excluded pointer fields assumed four bytes | in-memory galaxy obfuscation during save/snapshot and restore | compare object layout and protected fields before changing | confirmed ARM64 hazard; a width-only replacement would still corrupt eight-byte VMT/fields | OPEN |
| `aGalaxy.cpp:1207-1400` load-transition ID cells | serialized 32-bit object IDs temporarily held in list/pointer slots until `IdTo*` resolution | on-disk IDs, not process addresses at these call sites | preserve ID semantics | do not widen disk IDs; inspect each transition for lifetime/order errors | CLASSIFIED for listed transition sites; runtime pending |
| `aScript.cpp:547,1462,1496,2151-2161,3044-3127,4052-4217` script values | actual object pointers narrowed for `lvDword` script variables and bindings | live script object references | FPC reference needed for script value ABI | requires pointer-safe script value representation; cannot reinterpret as 32-bit serialized IDs wholesale | OPEN |
| `aScript.cpp:4466-4507` UI cache order | actual `TBlockParEC*` narrowed to 32-bit comparison key | in-memory sort/search | none needed | compare `uintptr_t` keys or pointers with a defined strict order | OPEN |
| `runtime/windows.hpp` ordinal import | `uintptr_t` | DLL ordinal selector | native loader | remove DLL path on Switch | TODO |
| `CrcUnit` | `void*`, `int32_t` byte count | transient bytes | portable | unchanged; never serialized | PASS |

First validated release package has `entry_record_size=158`; direct `sizeof`/layout reuse is prohibited for package structs containing pointers.
