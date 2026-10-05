# ABI audit

| Interface | convention/data | category | Switch action | status |
|---|---|---|---|---|
| internal translated calls | `PAS_STDCALL`, Delphi receiver lowering | internal-only | remove/ignore decoration only while preserving call order | pending |
| `okgf.dll` | exported renderer functions / pointers | external DLL ABI | link portable source behind adapter | pending |
| `MatrixGame.dll` | Win32/D3D9 callbacks | external DLL ABI | separate reconstruction; never load PE on Switch | deferred |
| Vorbis/Xvid/AVI DLLs | dynamic loader APIs | external codec ABI | static decoder or explicit optional adapter | pending |
| package records | fixed 32-bit fields, 158-byte entry | serialized ABI | fixed-width disk decoder, no host pointers | first stage PASS |

No generic `LoadLibrary` or calling-convention compatibility shim is introduced.
