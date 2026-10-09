# Startup dependency chain

| Level | Source/functions | Dependencies | Switch blocker | Status |
|---|---|---|---|---|
| entry | `src/program.cpp`: `main` → `Rangers::ProgramMain` | full translated runtime | pulls all Win32/UI units | not entered |
| runtime | `runtime_support.hpp`, `runtime/*.hpp` | Delphi RTL, x87 helpers, Win32 imports | x87 asm, `windows.hpp` imports | subset compiled |
| game globals | `Rangers.cpp`: Forms, `GR_Main`, globals | UI, threads, audio, registry | Win32 events/window/registry/Steam | blocked after CrcUnit |
| filesystem | `EC_File.cpp` → `TPackCollectionEC` | logical handles, package collection, lock | original `EC_HsFile.cpp` HANDLE/loose-file implementation | Milestone 4: replace only collection backend with Package adapter |
| resource slice | `TFileEC::TryAcquireReadHandle/ReadBuffer/SetPointer` | portable adapter, `Package`, runtime strings | none on reached packaged path | PASS on host: `DATA/Asteroid/00.gai`, CRC/seek differential |
| GR_Main configuration | `GR_Main::LoadLanguageAndPackages` → `LoadSelectedModInstallBlocks` → `aPacket::LoadConfiguredPackages` | `EC_Buf`, `EC_BlockPar`, `EC_Str`, `EC_Mem`, real globals, install/language/mod config, multi-package collection | common game-root resolver backs loose files and `FileExists` | Milestone 6: PASS on host for synthetic selected-mod precedence and release Russian configuration with 18 ordered sources |
| platform runtime bootstrap | `startup_slice` → real `aPacket::InitializePackageCollection` → real install load | portable timing, lifecycle cleanup, logical window token | SDL2 window on Switch; headless token in CI | Milestone 7: host lifecycle/failure PASS; ARM64 NRO build PASS |
| SDL window | `runtime_platform::CreateMainWindow` | SDL2 video + gamecontroller runtime | physical event loop is bounded to M12 diagnostic loop | M7: HOST/CI/ARM64/HARDWARE PASS |
| DAT roots | `GR_Main::LoadDatConfigAndModOverrides` | `EC_Data`, encrypted DAT loader, `CCInterface`, configured package collection | none on the reached portable path | M9: HOST/CI/ARM64/HARDWARE PASS |
| user settings | `runtime_settings_slice` → `CFG.TXT` | contained writable user `config/`, `TFileEC::CreateNew` | no Win32 Documents/registry dependency | M9: HOST/CI/ARM64/HARDWARE PASS |
| runtime config state | DAT roots → `GameDataConfig`, `UiStyleConfig`, `UiDepthConfig`, `WideCaseTable` | real `Main.dat`, language DAT, cache root | GlobalCache/audio/global UI intentionally not entered | M9: host/CI/ARM64 PASS |
| renderer bootstrap | `GR_DXInit` and Direct3D/OKGF bridge | renderer device and presentation | Direct3D semantics have no reached portable backend | M8: PASS |
| M13 metadata/bitmap slice | `ui_metadata_slice` and direct `TCBitmapEC::LoadFromConfigBuffer` | M9/M11 roots, CPU OKGF bridge, existing software renderer | no Forms, worker, D3D surface, script or audio entry | COMPLETE / HARDWARE NOT REQUIRED |
| M14P GI Format-0 | `GlobalCache::OpenDataBuffer` → structural validator → CPU decoder | M11 cache, OKGF RGB565 bridge | direct M14 GR_DX and M14R `TGraphBufGR` routes remain excluded | HOST/CI/ARM64/HARDWARE PASS; 93x104 BGRA fingerprint verified |
| M15 GAI Format-0 frame | `common.pkg` → GAI validator → frame directory → raw/ZL payload → M14P decoder | package reader, zlib bridge, M14P | no `TCGaiEC`, `GI_GAIFile`, surfaces, Direct3D, playback, or UI | RELEASE BASELINE/CI/ARM64/HARDWARE PASS; fixed 2000x2000 BGRA fingerprint verified |
| M16 GAI Format-2 frames | `common.pkg` → GAI validator → 100 raw frames → RLE validator → OKGF CPU draw | package reader, M16 decoder, OKGF RLE | no `TgiGR`, `TGraphBufGR`, Direct3D, playback, cache or UI | HOST/CI/ARM64/HARDWARE PASS |
| M17 embedded GAI playback | sequence 0 → portable state → selected frame → M16 Format-2 decode | M15 parser, M16 decoder, M12 opt-in monotonic callback | no UI, compositing, cache, audio, `GI_GAI` or Direct3D | HOST/CI/ARM64/HARDWARE PASS; one full 5000 ms cycle matched oracle |
| M18 software compositor | decoded BGRA source → clip/alpha CPU compositor → existing RGB565 `ScreenRenderBuffer` → existing presentation | M15 package reader, M16 decoder, M12 opt-in draw callback | no `TGraphBufGR`, `TgiGR`, `GI_GAI`, UI/cache/audio or Direct3D | HOST/CI/ARM64/HARDWARE PASS; visible 33x40 Asteroid screenshot and clean M12 lifecycle verified |
| M19 scene compositor | two real decoded GAI frames + alpha overlay → stable layer ordering → M18 CPU RGB565 compositor → existing presentation | M15 package reader, M16 decoder, M18 compositor, M12 draw callback | no UI/Forms/game objects/input/cache/audio/Direct3D | COMPLETE — host/CI/ARM64/oracle/hardware PASS |
| M20 GI object layer | package resource → `GIObject` metadata/current frame → M17 playback advance → `GIObject::Draw(Scene)` → M18 CPU RGB565 compositor | M15 package reader, M16 decoder, M17 playback, M19 scene, M12 callbacks | no Forms/UI/game objects/`EC_Cache`/threads/audio/Direct3D; no global image cache | COMPLETE — HOST/CI/ARM64/HARDWARE PASS; Switch build `73340fc`, 172,463 ms, PLUS/BOOT COMPLETE |
| M21 static image foundation | package resource → `PortableImageObject` → Simple/Trans/Alpha CPU representation → layout → M19 framebuffer | portable OKGF, M18 compositor, M20 retained animated scene | no Forms/UI controls/cache/worker/Direct3D; release Trans/Alpha are not substituted when absent | COMPLETE — host/CI/ARM64/oracle/hardware PASS; Switch build `fc8adfc`, 172,747 ms, PLUS/BOOT COMPLETE |
| M22 UI object/layout | config adapter → `UiTree` parent/child geometry/depth/clip/Panel scroll → image/GI leaves → RGB565 framebuffer | M20 `GIObject`, M21 `PortableImageObject`, M12 callback | no MessageLoop, Forms/events/text/cache/Direct3D; incremental scroll optimization deferred | COMPLETE — host/Python oracle, CI `37783935697`, clean ARM64/symbol audit and Switch hardware PASS; `de807a7`, 111,702 ms, PLUS/BOOT COMPLETE |
| M23 AFT/text/Label | M13 font cache key → validated AFT → tagged layout → `UiLabelLeaf` in M22 tree → RGB565 framebuffer | M13 metadata, M21 image, M22 tree, M12 callback | no `TCFontEC`, font worker, system fonts, embedded-control callbacks, Forms or Direct3D | COMPLETE / HARDWARE PASS in cumulative M24 Switch run; CI `37803515979`, clean ARM64 and historical RC recorded |
| M24 GraphButton/Window/Zone | M22 tree + M21 images + M23 caption → portable state, border layout, hit tests → RGB565 framebuffer | M21 image, M22 tree, M23 font/Label, M12 callback | no input dispatcher, callbacks, button audio, GI generic image substitution, Forms or Direct3D | COMPLETE / HARDWARE PASS; CI `37823081354` and `37825116392`, clean ARM64, symbol audit and cumulative Switch log PASS |
| audio | DirectSound path | audio device | intentionally deferred | DEFERRED |
| resource | `CrcUnit.cpp`: `ComputeCrc32` | portable Delphi helpers | none on compiled path | PASS: linked ARM64 |
| renderer | `GR_Main`, `EC_OKGF`, OKGF | window, `okgf.dll` ABI | game-facing adapter incomplete | portable OKGF is fully built, linked and fills a CPU framebuffer |
| main game | `Rangers::ProgramMain` | all prior layers | prior blockers | not entered |

The NRO runs the portable platform startup then the real GR_Main configuration slice before its disk-layout-compatible `EC_HsFile` resource self-test against `DATA/common.pkg`; the bounded hardware diagnostic has passed. `Rangers::ProgramMain` and `GR_Main::InitializePlatformRuntimeAndMainWindow` are not entered.

## M12 runtime loop

After M9/M11, the portable path applies release-compatible scalar CFG values (display/robot brightness and contrast, 3D, requested multithreading, path/mouse and clamped audio/music settings) without registry, OS, affinity, module or memory probing. `GR_DXInit` creates the existing RGB565 backend; M12 sets screen centres and the release `InterfaceBlendPalette`, then `runtime_loop_slice` pumps events, polls `PLUS`, draws an isolated heartbeat and presents through `GR_Main::BeginFramePresentation` / `EndFramePresentation`.

Audio and music settings remain requests only; no DirectSound, Vorbis, audio thread or controller is constructed. `Globals::InitializeGlobalUiRuntime` and `Rangers::ProgramMain` remain deferred.

## M17 diagnostic hook

The normal M12 loop has no callback by default. For M17 it invokes one opt-in diagnostic callback once after input/exit polling and before heartbeat presentation, passing the same monotonic `now_ms` that it uses for loop accounting. A callback failure is reported as `diagnostic_failure`, not as a renderer failure. The M17 callback decodes only initial and transitioned frames; it does not draw them.

## M18 draw hook

After the heartbeat, but before the existing presentation, M12 optionally invokes a separate draw callback. M18 uses it to alpha-composite the fixed decoded Asteroid frame in the centre of the existing RGB565 framebuffer. This preserves M12 behaviour whenever no draw callback is installed and preserves the M17 timing callback and `PLUS` polling order.

## M19 scene draw hook

M19 reuses that single opt-in draw point after the M12 heartbeat. It clears no extra state and keeps M12 input, timing and presentation intact; each presentation re-renders the three current CPU sprites in stable layer order. It owns only those decoded images and has no cache or background work.

## M20 GI object hook

M20 keeps the same callback order. The frame callback advances M17 evidence and all three GIObjects with the same monotonic tick; the draw callback clears the transient scene and calls each object's `Draw`. Thus resource selection, decoded-frame ownership and animation state stay with GIObject, while Scene only represents the current frame for the existing CPU compositor.

M21 adds `PortableImageObject` at that draw boundary without changing renderer ownership. The final Switch run verified the real Simple `DATA/Planet/Spu00.png`, declared Trans and Alpha `NOT_PRESENT` from the release inventory rather than inventing replacements, and matched the fixed RGB565 checkpoint `4f915772`/`52449ae8f8f56c6c`. It retained M20 animation and M17 completed one 5,008 ms cycle before the user exited through `PLUS`.

M22 replaces the manual runtime presentation entries with a portable `UiTree` while retaining the M12 callback boundary. It adds nested panels, exact depth traversal, clipping and scroll geometry around the M20/M21 resource leaves. Its fixed runtime oracle, terminal CI, clean ARM64 audit and Switch run have passed.

M23 uses the same tree and callback boundary. Before the dynamic loop it loads a
real Russian `WinText` Label through the config adapter and cache-key AFT
resolver, then checks font, UTF-16 text, tree and RGB565 frame fingerprints
against an independent Python oracle. During the loop the prepared Label is
drawn after the retained M21/M22 first-frame checksum, so their established
checkpoint remains separately verifiable. The cumulative M24 Switch run matched
that checkpoint and completed the M23 dynamic stage.

M24 runs its fixed in-memory Window/GraphButton/Zone checkpoint after the M23
fixed real Label check and before the dynamic loop. The M24 Window layout,
Zone hits, tree, GraphButton state, and RGB565 frame match independent Python
hashes. Its dynamic control scene then uses the existing M12 update/draw hooks
and cycles Normal/Hover/Down/Disabled while M23 and earlier diagnostics remain
active. The cumulative M24 Switch run matched the fixed checkpoint and
completed the dynamic M24 stage, PLUS exit, and orderly shutdown.

## M25 GI/GAI resource layer

M25 extends the optional portable UI path with raw GI static images and a
separate GAI control, retaining M14P/M16 decoding, M17 timing and M18 RGB565
composition. The selected Main.dat `PLBar` subtree can now be constructed and
rendered locally from real package resources through `CacheDataRoot` without
invoking Direct3D or the original GI message loop. M25 host, CI and clean
ARM64 gates passed. The supplied physical Switch log confirms the cumulative
M23/M24/M25 runtime checkpoints, live GAI advance, 1,303 presents, `PLUS` and
clean shutdown. The supplied screenshot visibly shows the first real `PLBar`
subtree. M25 is COMPLETE / HARDWARE PASS.

M12 hardware PASS: the M14P-tested NRO embedded `46330b4` ran for 73.472 seconds, presented 1,506 frames, exited through `PLUS`, and reached `[BOOT] COMPLETE`. Before the loop it also completed the M13 metadata diagnostic and M14P `Bm.Captain.2BlazerBi` Format-0 decode with the recorded 93x104 BGRA fingerprint.
