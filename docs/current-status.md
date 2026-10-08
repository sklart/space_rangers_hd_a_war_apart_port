# Current status — M24 COMPLETE / HARDWARE PASS

M24 adds portable GraphButton, Window, and Zone controls on the M22/M23 tree.
The local retained host regression, independent three-state Python oracle,
read-only release inventory, and portable-object symbol audit pass. Physical
release instances are GraphButton 619, Window 42, Zone 61; all real GraphButton
state images and Window border images use unsupported generic `GI` mode, while
all Zones are eligible. A fully supported real subtree is formally NOT FOUND.
Terminal CI `37823081354` (production) and `37825116392` (final software
commit) passed the retained M12–M23 suite, M24 oracle and
symbol audit. A subsequent clean ARM64 build produced ELF64 AArch64 with zero
undefined symbols. The cumulative 7,698,736-byte NRO embeds `build_git=00808c1`
and has SHA-256
`93E7A6AA4B4EE71A9C1F6AE93F72B75F63E193152D61F944E4DA2947187B3C89`.
The user-supplied physical Switch `port.log` reports `build_git=00808c1`,
matching M23 and M24 fixed checkpoints, 1,322 frames/presents in 66,151 ms,
`exit_reason=plus`, all shutdown stages, and `[BOOT] COMPLETE`. The M24
dynamic render gate passed. M24 is **COMPLETE / HARDWARE PASS**. The SD copy's
SHA-256 was not independently measured after manual transfer. See
[the M24 evidence document](milestone24-ui-controls.md).

M23 software is complete in a separate worktree. Synthetic AFT/tagged-text/Label
host gates, the independent mixed-tree Python oracle and the Russian real
`Font.2Intro` Label oracle pass locally. All 17 release AFT resources validate
with 3,831 glyphs and zero duplicate codepoints. The selected real Label's
fixed tree and RGB565 frame fingerprints are known before Switch access:
`ef3ef436`/`6022c76fb3cb9306` and `b36cfe2f`/`6b916c3b29d2a194`.
The runtime stage now compares these values before the M12 dynamic loop.
GitHub Actions run `37803515979` passed M12–M23, the Python oracle and M23
symbol audit. Clean ARM64 produced ELF64 AArch64 with zero undefined symbols.
The RC NRO embeds `build_git=19eb6a5`, is 7,641,392 bytes, and has SHA-256
`877F15D0B6187B033490815FBBC21229983AA9040FAE27B2F395995C528A2792`.
The same cumulative M24 Switch run matched the M23 font/text/tree/frame
checkpoint and passed its dynamic render gate. M23 is **COMPLETE / HARDWARE
PASS**; its earlier standalone RC remains a historical unrun artifact.
See [the M23 evidence document](milestone23-font-label.md).

M20 remains complete. M21 is **COMPLETE**: the CPU foundation, release-config
inventory, independent Simple Python oracle and synthetic mixed M20/M21
frame-A/frame-B checkpoint passed on host and in CI. The final hash-bound
Switch NRO embedded `build_git=fc8adfc`, was 7,547,184 bytes with SHA-256
`6D387610DAD7DA57C1D9F21F00ABF84C67F9AA266421CF54C98567656C8FA3F1`, and
ran for 172,747 ms with 3,824 frames/presents before `PLUS` and `[BOOT]
COMPLETE`. Its actual M21 scene fingerprint matched the independent oracle:
`CRC32=4f915772`, `FNV64=52449ae8f8f56c6c`. CI run `37770073920` passed the
retained gates, M21 regression and M21 symbol audit.

M22 is **COMPLETE**. Its portable object/layout core, config adapter,
mixed C++/Python tree oracle, read-only release inventory and runtime `UiTree`
integration exist. GitHub Actions run `37783935697` passed the retained suite,
M22 regression, tree oracle and release-probe syntax check. A clean ARM64 build
of `de807a7` produced ELF64/AArch64 with zero undefined symbols and zero
forbidden-symbol hits across the new M22 objects. Its NRO is 7,559,472 bytes,
SHA-256 `ED08444B2B41056C214D8A809E4FA2A5B72C31E372844119191D68863FCC3716`,
with embedded `build_git=de807a7`. The sole Switch test passed: tree and frame
fingerprints matched, all depth/clip/scroll/active invariants passed, M17's first
cycle took 5,039 ms, M12 ran 2,277 frames/presents over 111,702 ms, then `PLUS`
led to `[BOOT] COMPLETE`; `gr-main.log` contains only `Start`.

The synthetic presentation checkpoint uses one M20 GIObject plus M21 Simple,
keyed Trans and partial-alpha Alpha in stable layer order. Its C++ and
independent Python oracle both record frame A `1a829653`/`658ac816b5479db3`
and frame B `383a8729`/`69ffb049d0071ebb`; it is not a real-release scene.

M21 inventory resolves `Bm.Planet.T.Spu00` to `DATA/Planet/Spu00.png` in
`common.pkg`; the independent PNG/Adam7 RGB565 oracle records source
`a3721a9c`/`32ebfdd05d7fa674` and decoded `51e16db2`/`ffeaf550d3c28655`.
The base release has no `Trans` reference. Its six `Alpha` `Bitmap.BGObj.*`
references have no mapping in base `CacheData.dat` or language DAT, therefore
they are release **NOT PRESENT**, not substituted with arbitrary assets.
The M21 runtime integration verifies the same Simple source and decoded
fingerprints, then compares its fixed first mixed M20/M21 frame with the
independent `4f915772`/`52449ae8f8f56c6c` oracle. Earlier runs `6fb6e98`,
`4c7012b` and `82aa2e9` remain diagnostic failures only: they corrected the
RGB565 expansion and full-height heartbeat-marker modelling in the Python
oracle. The final `fc8adfc` hardware run is the only M21 PASS evidence.

Baseline SHA-256: `83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.

- M7 startup/platform: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M8 RGB565 + SDL presentation: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M9 DAT, CFG and derived runtime state: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M11 GlobalCache and cached real resource: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M12 runtime loop: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M13 UI metadata: **COMPLETE / HARDWARE NOT REQUIRED**.

## Milestone 13

M13 is deliberately limited to CPU-side UI prerequisites.  It initializes the 17 release font metadata keys from `GlobalsV`, reads `FontSmooth` only from the portable user CFG, records each `CacheDataRoot` resolution, and owns no cache/root/renderer object.  A later `SetCacheKey` retains upstream alias behavior: the four UI font aliases resolve to the smooth keys only when `FontSmooth=True`.

Bitmap loading is direct and bounded: `GlobalCache::OpenDataBuffer` supplies a buffer to `TCBitmapEC::LoadFromConfigBuffer`; it does not enter worker/cache-control acquisition.  The OKGF bridge rejects malformed or oversized dimensions before allocation, leaves rejected `TGraphBufGR` instances at `Width=0`, `Height=0`, `Pixels=nullptr`, `ResidentBytes=0`, and cancels the codec context exactly once.

Status: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE NOT REQUIRED**. The local release probe passed against the original game tree in Ubuntu 26.04 WSL with GCC/G++ 15.2.0, CMake 4.2.3, libpng 1.6.57 and libjpeg 2.1.5. It selected `Bm.PQI.Build_00` (`data\PQI\Build_00.jpg`): source 39,671 B; decoded 343x394, 2 BPP, pitch 688, resident 271,072 B, CRC32 `a2870aca`, FNV-1a-64 `04bb5ed285e9e0d8`. Repeat decode matched; all 17 real font metadata resolutions were found with existing backing files.

M13 does not construct Forms, screen objects, script runtime, audio, DirectSound, Steam, registry access, `Rangers::ProgramMain`, or the full global UI initializer. M14 direct and M14R GraphBuf approaches remain blocked; M14P portable Format-0 is the active solution.
M12 hardware provenance: tested NRO embedded build `63e1f5f`; the subsequent lineage contains documentation and host-only test infrastructure only, so no ARM64 runtime input changed and this hardware result applies to the current runtime state.

## Milestone 14

M14 direct upstream `TgiGR::DecodeToGraphBuf` remains **BLOCKED** by the broad `GR_DX` fan-out. M14R remains **BLOCKED** because `TGraphBufGR` owns Direct3D `IDirect3DTexture9` COM state. M14P is the successful portable Format-0 CPU decoder and creates neither `TgiGR` nor `TGraphBufGR`.

M14P status: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**. The hardware-tested NRO embedded `build_git=46330b4` and decoded real key `Bm.Captain.2BlazerBi` as 93x104 BGRA with pitch 372, CRC32 `cf5b1d56`, and FNV-1a `a668e341bc42a6fb`. The same run completed the M12 loop with 1,506 frames/presents in 73,472 ms, exited through `PLUS`, and reached `[BOOT] COMPLETE`.

M15 status: **RELEASE BASELINE PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**. The hardware-tested NRO embedded `build_git=9d8d8bb` and SHA-256 `EC4B1936E2CC7C0E39EBB9DF20FCD60979168BD05D283DDF3C91C5437C695D01`. It parsed `DATA/BGObj/bg00.gai`, frame 0, and matched the oracle GAI CRC32/FNV `9e05776f`/`03f332f6307d4031`, GI CRC32/FNV `05d665d2`/`3ccdac34b2d0a2cc`, and 2000x2000 BGRA CRC32/FNV `3fc81562`/`ad9d67c6c7ad85b9`; M12 then ran 2,003 frames/presents for 100,264 ms, exited through `PLUS`, and reached `[BOOT] COMPLETE`. M15 keeps GAI parsing and one selected frame fully CPU-only: bounded header/directory/sequence validation, bounded raw/ZL01/ZL02 extraction, then the existing M14P Format-0 decoder. The independent release oracle records that `DATA/Asteroid/00.gai` is Flags=0 but Format-2-only; deterministic lexical fallback selects raw Format-0 `DATA/BGObj/bg00.gai`, frame 0, whose 2000x2000 BGRA fingerprint is recorded in `milestone15-gai-cpu.md`. Animation, timing, cumulative Flags!=0 composition, surfaces and cache/UI workers are deliberately absent.

The M14P-tested NRO was `Space Rangers HD - A War Apart.nro`, 7,158,064 bytes, SHA-256 `1CDFFC96A8505BA6D9114222F46CF32B4A16E80C93C93735D13198293E61D97E`. `port.log` proves the embedded build Git; deployment preflight/manifest proves the NRO SHA-256. M14 and M15 are **COMPLETE**.

M16 status: **COMPLETE — HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**. GitHub Actions run `37623625785` passed the M16 GI Format-2 CPU regression and its separate symbol audit together with all retained M12–M15 gates. On Switch, the tested NRO SHA-256 `470246273738C66777672E0D88A3449DA3FF47398AC08C16BB3175B5CAE8E08E` (`build_git=50b778c`) decoded all 100 Asteroid frames with matching frame-0 and aggregate fingerprints, then M12 ran for 47,272 ms and 943 presents before `PLUS` and `[BOOT] COMPLETE`. M16 decodes all 100 raw Format-2 frames of `DATA/Asteroid/00.gai` with bounded RLE validation and the portable OKGF CPU path. Frame 0 and the all-frame canonical fingerprint are fixed in `milestone16-gi-format2.md`. The new M16 ARM64 objects pass the forbidden-symbol audit; the full inherited runtime ELF retains pre-existing `GR_DX`/`TGraphBufGR` renderer symbols.

M17 status: **COMPLETE — HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**. GitHub Actions run `37736270948` passed retained M12–M16 gates, M17 sequence-parser/playback regressions and the M17 symbol audit. The tested NRO was 7,194,928 bytes, SHA-256 `E753BFEAA6BF71C48086C98BB8A27307A0929ECB05B362D343CFA7B8777DCD0D`, embedded `build_git=13ad513`. On Switch it read only embedded `Flags==0` sequence 0 from `DATA/Asteroid/00.gai`, decoded selected Format-2 frames through M16, matched sequence CRC/FNV `4b1c6ebf`/`47cdc8c73fc1ce61` and cycle CRC/FNV `5b7bc7e9`/`f70813ac799a25b3`, completed the 5000 ms nominal cycle in 5033 ms with max tick gap 129 ms and tolerance 134 ms, then M12 ran 832 frames/presents for 41,746 ms, exited through `PLUS`, and reached `[BOOT] COMPLETE`. M17 remains CPU-only and is not a UI animation, compositor, cache, audio or text-sequence implementation.

M18 status: **COMPLETE — HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**. GitHub Actions run `37739330544` passed the retained regression suite, the M18 software-compositor test and its separate forbidden-symbol audit. The tested 7,199,024-byte NRO had SHA-256 `D2322AB4881796FFE3EB55CF493A6191A54547B636EBA10C24EC3FBF91BE9DDB` and embedded `build_git=f9833a5`. On Switch it decoded source frame 0 from Asteroid sequence 0 as 33x40 BGRA/pitch 132 (`CRC32=83f66519`, `FNV=eb000366ca288b23`) and alpha-composited it at 623,340 in the 1280x720 RGB565 framebuffer. The supplied screenshot confirms the visible Asteroid over the heartbeat. `port.log` records `[M18] compositor PASS`, M17's matching cycle CRC/FNV and 5042 ms timing, then 3,331 frames/presents for 166,725 ms, `PLUS` and `[BOOT] COMPLETE`. M18 creates no game UI, cache object, Direct3D/`TGraphBufGR`/`TgiGR`, `GI_GAI`, audio or input subsystem.

## Milestone 19

M19 status: **COMPLETE — HOST / CI / ARM64 / SWITCH HARDWARE / PYTHON ORACLE PASS**. It replaces M18's single draw with three current-scene sprites: `DATA/Asteroid/00.gai` frame 0, the first other decodable `DATA/*.gai` in lexical order, and a 50%-alpha Asteroid overlay. Scene ordering is stable ascending by layer; x/y are top-left coordinates and clipping delegates to the M18 CPU compositor. `host-scene-compositor-test` proves layering, exact alpha RGB565 result and fingerprints, clipping, visibility and deterministic rerender. CI `37745110922` on `e2c3556` passed the retained suite, M19 scene compositor regression and M19 forbidden-symbol audit. The Switch-tested NRO SHA-256 is `E05B83F0675548D25911AC497541EDFF1C8D0EE66CD5FD26922FA9D57EE131FF`, embedded `build_git=3b337cd`; it selected `DATA/Asteroid/01.gai`, logged scene CRC32/FNV `ba977214`/`240b58539a257627`, displayed multiple real resources, then ran 594 frames/presents for 29,993 ms, exited via `PLUS` and reached `[BOOT] COMPLETE`. The independent oracle was run against `windows/Space Rangers HD A War Apart`: it selected the same three sprites and exactly matched the canonical scene (`14,244 bytes`, CRC32 `ba977214`, FNV64 `240b58539a257627`).

## Milestone 20

M20 status: **COMPLETE — HOST PASS / CI PASS / ARM64 BUILD PASS / SWITCH HARDWARE PASS**. `GIObject` owns one resource's parsed GAI state and its current CPU frame. Main constructs three objects from three real decodable `DATA/*.gai` resources, advances all of them by the M17 playback primitive and supplies the M19 scene only through `GIObject::Draw`; it does not construct `SceneSprite` from decoded pixels. The host test verifies load, animation, layer, exact RGB565 alpha, visibility and deterministic scene state. GitHub Actions run `37753205358` passed the retained suite, the M20 GI-object regression and its forbidden-symbol audit. The hash-bound Switch NRO embedded `build_git=73340fc`, had SHA-256 `0F7B909287B28B4D6DD7CE3617DF73E869907ED9F328F9E6A71AF81970523243`, created Asteroid `00.gai`, `01.gai` and a 50%-alpha `02.gai` overlay, passed M20 and M17, ran 3,824 frames/presents for 172,463 ms, then exited through `PLUS` and reached `[BOOT] COMPLETE`. `EC_CacheAlphaBitmap` was analysed only: its cache controls, `TGraphBufGR`, `GR_DX` texture cache and Direct3D path remain excluded for M21 planning.
