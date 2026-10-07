# Current status — M13 metadata and bitmap decode boundary

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

M16 status: **IMPLEMENTED / HOST PASS / ARM64 BUILD PASS / HARDWARE PENDING**. It decodes all 100 raw Format-2 frames of `DATA/Asteroid/00.gai` with bounded RLE validation and the portable OKGF CPU path. Frame 0 and the all-frame canonical fingerprint are fixed in `milestone16-gi-format2.md`. Animation playback, sequence timing, Flags!=0 composition, non-Format-2 GI, surfaces/textures and UI are not implemented.
