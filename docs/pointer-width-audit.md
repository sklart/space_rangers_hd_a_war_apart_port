# Pointer-width audit

| location | original type | semantic purpose | FPC solution | C++ Switch solution | status |
|---|---|---|---|---|---|
| `TPackEntryEC` | serialized 158-byte entry plus `ChildFolder*` | package directory record | retain disk layout | parse disk fields explicitly; no host pointer deserialization | PASS for root record |
| `TPackOpenSlotEC` | `uint32_t FileHandle`, buffers | runtime handle/buffers | platform wrapper | replace Win32 handle before collection port | TODO |
| `aGalaxy.cpp` object-id casts | pointer stored in `uint32_t` | tagged IDs / load transition | reference needed per use | classify ID cell vs pointer; no global widening | TODO |
| `runtime/windows.hpp` ordinal import | `uintptr_t` | DLL ordinal selector | native loader | remove DLL path on Switch | TODO |
| `CrcUnit` | `void*`, `int32_t` byte count | transient bytes | portable | unchanged; never serialized | PASS |

First validated release package has `entry_record_size=158`; direct `sizeof`/layout reuse is prohibited for package structs containing pointers.
