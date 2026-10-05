# Static initialization boundary after the portable startup slice

Milestone 7 adds a portable runtime boundary before the real GR_Main
configuration slice. It owns timing, a logical main-window token and SDL
window creation on Switch; host CI uses the equivalent headless token. It
then calls the real package collection, loads `INSTALL.TXT`, runs the real
language/mod package path, and creates `MessageText::TQuestMessages`.

`GR_Main::InitializePlatformRuntimeAndMainWindow` remains discarded from the
final ELF. Its Windows-only DLL probing, COM, window class/timer and Forms
assignment are not emulated. Direct3D renderer bootstrap is the next reached
boundary; audio remains deferred, and `Rangers::ProgramMain` is not entered.

The remaining three small registry return-value hooks are only translation
linkage for discarded legacy failure branches; they are not reached and do not
model registry behaviour. The game-root resolver and `SysUtilsImports::FileExists`
are portable filesystem operations, not a Win32 compatibility layer.
