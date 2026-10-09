# E2E-WIN32 — Windows API surface audit

Status: the Win32 surface sweep passes source inventory, host tests, ARM64 link and the supplied Switch run through the original screen loop. The latest Switch run rendered the loading screen and main menu, but stick, D-pad, A and PLUS had no visible effect. Input handling and clean exit remain unverified. This report describes the pinned C++ source `57fa689c630193a66fdea6ca4c79a188814991cd` and disposable E2E source overlay.

## Source inventory and classification

| Source view | Static imports | Dynamic module/procedure calls | Direct DLL declarations | DLL literals | Unparsed | UNKNOWN | Startup REQUIRED_UNIMPLEMENTED |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pinned upstream | 256 | 81 | 17 | see `win32-import-manifest.json` | 0 | 0 | 0 |
| Effective generated Switch source | 254 | 69 | 0 | 22 | 0 | 0 | 0 |

The effective view has 137 `PORTABLE`, 88 `STATIC_LIBRARY`, 76 `OPTIONAL_DISABLED`, 15 `NOT_REACHED_UNTIL_GAMEPLAY`, and 7 `RUNTIME_MODULE` entries across static and dynamic callsites. The static source may spell `kernel32` without a suffix; the resolver normalizes it to `kernel32.dll`. The scanner records variable `LoadLibrary` and `GetProcAddress` arguments as `DYNAMIC_MODULE_EXPRESSION` with their callsites. Its synthetic test covers literal and variable loads, imports, an ordinal, direct declarations, and conditional source selection. A dynamic expression is classified by its reachable policy; the exact runtime string supplied by a game script is not statically enumerable.

| DLL or callsite group | Total | PORTABLE | STATIC_LIBRARY | OPTIONAL_DISABLED | RUNTIME_MODULE | NOT_REACHED_UNTIL_GAMEPLAY | REQUIRED_UNIMPLEMENTED | UNKNOWN |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `kernel32.dll` (including `kernel32`) | 71 | 69 | 0 | 2 | 0 | 0 | 0 | 0 |
| `user32.dll` | 52 | 52 | 0 | 0 | 0 | 0 | 0 | 0 |
| `gdi32.dll` | 3 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| `advapi32.dll` | 6 | 6 | 0 | 0 | 0 | 0 | 0 | 0 |
| `winmm.dll` | 5 | 3 | 0 | 2 | 0 | 0 | 0 | 0 |
| `zlib.dll` | 3 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| `okgf.dll` | 88 | 0 | 88 | 0 | 0 | 0 | 0 | 0 |
| `avifil32.dll` | 8 | 1 | 0 | 7 | 0 | 0 | 0 | 0 |
| `dsound.dll` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `ole32.dll` | 4 | 0 | 0 | 3 | 0 | 1 | 0 | 0 |
| `shell32.dll` | 3 | 0 | 0 | 3 | 0 | 0 | 0 | 0 |
| `gdiplus.dll` | 10 | 0 | 0 | 0 | 0 | 10 | 0 | 0 |
| Dynamic module/procedure expressions | 69 | 0 | 0 | 58 | 7 | 4 | 0 | 0 |

Static DLL rows count unique DLL/API pairs; the dynamic row counts callsites. `UNKNOWN=0` means every statically visible callsite has a recorded decision. It does not mean disabled video/audio or deferred gameplay facilities have been implemented.

## Resolution path

The original `WindowsImports`, `WindowsSdk`, `SysUtilsImports`, and `VFW` wrappers call `pas::win::load_import`. The generated Switch runtime header routes that call to `win32_compat::ResolveImport`, which normalizes DLL names, resolves a typed platform thunk, and logs an unknown required DLL/API with the current E2E stage before throwing. An ordinal selector is logged as `#number` without dereferencing it. Generated resolver cases exercise every unique `PORTABLE` static DLL/API pair and every `OPTIONAL_DISABLED` pair in the baseline and effective manifests. AVI opening/decoding, DirectSound, OLE, shell, and Windows-only locale imports throw a labelled optional error. `AVIFileExit` resolves to safe cleanup of a never-opened AVI session because original `TxvidGI::XvidClose` calls it during UI startup even when no video was opened. The kernel32 system-directory/affinity and WinMM timer probes resolve to thunks with explicit failure returns. The host test compiles the original wrapper translation units and exercises file, event, thread, calendar, window/message, registry, module, and procedure calls through them. The physical DLL load count on Switch is defined as zero; loaded builtins receive synthetic 32-bit module IDs.

Synthetic file, event, thread, window, module, GDI, find, global-memory, timer, and registry handles come from a thread-safe monotonically increasing table. Zero and `0xffffffff` are excluded. Lookup checks the handle type, close removes the entry, and stale tokens are not reused. Actual `GetProcAddress` results are callable pointers; only module handles are 32-bit tokens. The handle-width grep rejects pointer-to-32-bit casts in the compatibility implementation.

The ad-hoc API-function replacement inventory in `tools/e2e_source_overrides.py` fell from **35 functions in 5 source units** at hardware baseline `304e746` to **0 functions** after the resolver sweep. Disabled DirectSound enumeration stays in the original wrapper and would produce an `OPTIONAL_DISABLED` resolver error if the audio-off policy were violated. Startup, SDL window, user-root, and other platform-specific source adaptations are separate from these API replacements.

`okgf.dll` has 88 requested static exports, all bound to linked OKGF functions by the generated table. `steam_ach.dll`, Vorbis, Xvid, DirectSound and AVI loaders return missing-module status on the audio/video-off E2E path. Wine detection is disabled. `MatrixGame.dll` is requested in `Robot::InitializeRobotRuntime`; source inspection shows a missing module leaves `RobotInterface` null and permits menu initialization to continue. Robot battle gameplay therefore lacks that interface and remains unverified. Script-supplied module names in `EC_Expression.cpp` and `aScript.cpp` pass through the same module registry; an unrecognized required module fails loudly with DLL and stage, so future script content can expose a new requirement.

## Game-observable semantics and limits

- File operations resolve reads against the game install and user configuration roots and writes against the user root. CreateFile enforces observed read/write sharing conflicts among compatibility handles; concurrent compatible readers remain allowed. The wide CreateFile and FindFirst paths convert BMP UTF-16 names to UTF-8; a host fixture covers a non-ASCII filename. Original `SysUtilsImports::FindFirst` reaches the central resolver and uses the existing wildcard matcher.
- Events, workers, waits and close operations use the existing portable synchronization backends. A missing single-instance event returns absent, matching the Switch lifecycle policy.
- `TerminateThread` has one original caller in `Rangers.cpp:473`, during shutdown; it passes `TurnCalculationThread->IdleEvent`, an event rather than a thread handle, and ignores the return value. The compatibility layer rejects hard termination and logs an invalid token or unsupported live-thread request. The main-menu path does not rely on terminating a worker.
- QPC/frequency, Sleep, UTC/local calendar and FILETIME use portable clocks and calendar conversion. Host tests exercise a leap day and the original wrapper signatures.
- The registry is currently a process-local HKCU key-value store. HKLM writes are denied. It does not persist across restarts; original settings backed by separate config files still use the file path adapter. Cross-launch registry persistence remains a semantic difference.
- One SDL window backs logical HWNDs. Messages, timers and controller input are translated into the original WM-style queue. Host tests cover dispatch, pointer movement, A down/up and PLUS quit; Switch presentation and real original UI interaction require a hardware run.
- GDI cursors and DIB objects are synthetic resources. Audio, AVI, shell help, GDI+ and some OLE calls remain disabled or deferred under the recorded classifications. A call to an optional static import raises a labelled `OPTIONAL_DISABLED` error if that path is unexpectedly reached.

## Gates and evidence

`tools/audit_win32_surface.py --check` and `--effective --check` require current manifests, no UNKNOWN and no startup `REQUIRED_UNIMPLEMENTED`. `tools/generate_win32_portable_cases.py --check` requires the mapping fixture to match both manifests. `tools/audit_win32_handle_width.py` checks the compatibility layer's 32-bit handle conversions. `tools/e2e_win32_host_test.py` runs handle, filetime, window/message/input, resolver, file/registry and original-wrapper tests. CI runs these alongside the complete ARM64 E2E link and checks that `upstream/cpp` stays clean. A successful ARM64 ELF requires all 298 original units, PIE format, original `ProgramMain` and screen-loop symbols, and zero undefined symbols before NRO packaging.

At startup the generated entrypoint writes the exact effective-manifest file SHA-256 and its `PORTABLE`, `OPTIONAL_DISABLED`, and `UNKNOWN` counts to `[E2E][WINAPI]` lines. On normal completion and on caught failure it writes resolver counts for resolved and disabled imports, unmapped imports, dynamic module load attempts, and physical DLL loads. The build fails if the effective manifest changes after the stamp is generated.
Successful import calls remain quiet by default; a build compiled with `E2E_WINAPI_TRACE` records each resolved DLL/API and stage. Multi-type handle operations probe types without reporting a false invalid handle, then log one diagnostic if no valid type matches.

The supplied `build_git=242f07f` hardware log reaches `script host ready`, `runtime/settings`, `global UI`, `robot runtime`, and `entering RunMainScreenStateLoop`, then records `resolved=51 optional_disabled=0 unmapped=0 dynamic_loads=0 physical_dll_loads=0` and `BOOT COMPLETE`. The source log is [`e2e-eighth-hardware-242f07f.log`](evidence/e2e-eighth-hardware-242f07f.log), byte-identical to original `D:\e2e.log` SHA-256 `6A5169A7853F3F0076530BB1BDC80671D2B835B503C6341C96E931AE1423FAAF`. The user observed a black screen for about 1.5 minutes. This proves the Win32 startup path advanced beyond the earlier AVI failure and exited cleanly; it does not prove a frame was presented, a menu rendered, or input reached a game control.

The later `build_git=45e54da` hardware log shows `screenLoad` and main menu opening and returning, but `presents=0`; see [`e2e-ninth-hardware-45e54da.log`](evidence/e2e-ninth-hardware-45e54da.log). The original main-window activation callback had no synthetic Win32 window procedure to receive `WM_ACTIVATEAPP`, leaving `GR_Main::RuntimeActive` false and the original message loop unable to advance to drawing. The next candidate bound that procedure and posted activation through the compatibility queue; host dispatch tests and ARM64 link passed.

The `build_git=4e9cc05` hardware log reaches the load screen's first frame and the main menu's `OnOpen ready`; see [`e2e-tenth-hardware-4e9cc05.log`](evidence/e2e-tenth-hardware-4e9cc05.log). The user briefly saw an image resembling the menu background, then an error. The supplied `D:\crash_reports.zip` identifies Atmosphère `2168-0002` Data Abort in OKGF `alpha_rect`, called from `GI_GAI::TgaiGI::Draw`; the null pixel buffer came from a texture-backed graph buffer in software renderer mode. The Switch source overlay now allocates graph buffers in CPU memory; the original upstream file remains unchanged.

The `build_git=1f745ef` Switch run rendered both screens without the previous alpha-buffer crash; see [`e2e-eleventh-hardware-1f745ef.log`](evidence/e2e-eleventh-hardware-1f745ef.log). The user ended the unresponsive menu externally, so neither the original input path nor clean exit passed. The supplied screenshot also shows bottom controls clipped. The portable renderer omitted the original display-mode `ExtraScreenWidth/Height` layout offsets; these are restored in source for the next candidate, alongside bounded poll/dispatch/frame diagnostics. The previous NRO is **18,905,392** bytes with local SHA-256 `E70A4E7590E8C8CA2133B8505CD8BF25D2B4CF2163BBB14624031A9C09A6C01F`; its SD copy's SHA-256 is **NOT MEASURED**. The effective manifest file SHA-256 is `FC5FBD744721BCE9A65C4E2B012D9B5365181F3D113D9C317A58C6823FAB2F93`.
