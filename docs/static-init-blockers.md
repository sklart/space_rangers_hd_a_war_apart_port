# Static initialization boundary after the GR_Main configuration slice

Milestone 6 links the unmodified translated `GR_Main.cpp`, but section GC
retains only the executed configuration route: `AppendLogLineThreadSafe`,
`LoadSelectedModInstallBlocks`, and `LoadLanguageAndPackages`, together with
their globals. The unit's static initialization did not make a window,
Direct3D, DirectSound, registry, Steam, or threads mandatory.

`GR_Main::InitializePlatformRuntimeAndMainWindow` remains discarded from the
final ELF. It is still the next broad boundary: the function combines DLL
loading, window registration/creation, Direct3D, DirectSound, COM, timer,
Forms, package initialization and base configuration loading. M6 deliberately
does not call it or emulate its Win32 subsystem.

The remaining three small registry return-value hooks are only translation
linkage for discarded legacy failure branches; they are not reached by the
configuration slice and do not model registry behaviour. The game-root path
resolver and `SysUtilsImports::FileExists` are portable filesystem operations,
not a Win32 compatibility layer.
