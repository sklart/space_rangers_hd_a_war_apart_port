# Milestone 13 UI boundary

M12 deliberately does not call `Globals::InitializeGlobalUiRuntime()`.

| Group | State | Boundary |
|---|---|---|
| A. Scalar/user settings | M12 portable | CFG-only values, no Win32 probing |
| B. Font names | Needs work | resource/font lifetime and bitmap decoding |
| C. Cache loader/thread | Needs work | worker lifecycle and cache ownership |
| D. Specialized cache classes | Needs work | GI/GAI and data dependencies |
| E. Script runtime | Needs work | script DLL/runtime policy |
| F. Screen registration | UI dependency | Forms/screen object graph |
| G. Real first screen | UI and audio dependency | screen assets, input, sound/music |

The earliest M13 point is B: enumerate and load font-name metadata without constructing Forms, screen objects, cache threads, scripts or audio. No M13 implementation is included in M12.