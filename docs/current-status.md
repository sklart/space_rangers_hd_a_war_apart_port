# Current status — M13 metadata and bitmap decode boundary

Baseline SHA-256: `83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.

- M7 startup/platform: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M8 RGB565 + SDL presentation: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M9 DAT, CFG and derived runtime state: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M11 GlobalCache and cached real resource: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M12 runtime loop: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PENDING**.

## Milestone 13

M13 is deliberately limited to CPU-side UI prerequisites.  It initializes the 17 release font metadata keys from `GlobalsV`, reads `FontSmooth` only from the portable user CFG, records each `CacheDataRoot` resolution, and owns no cache/root/renderer object.  A later `SetCacheKey` retains upstream alias behavior: the four UI font aliases resolve to the smooth keys only when `FontSmooth=True`.

Bitmap loading is direct and bounded: `GlobalCache::OpenDataBuffer` supplies a buffer to `TCBitmapEC::LoadFromConfigBuffer`; it does not enter worker/cache-control acquisition.  The OKGF bridge rejects malformed or oversized dimensions before allocation, leaves rejected `TGraphBufGR` instances at `Width=0`, `Height=0`, `Pixels=nullptr`, `ResidentBytes=0`, and cancels the codec context exactly once.

Status: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE NOT REQUIRED**. The local release probe passed against the original game tree in Ubuntu 26.04 WSL with GCC/G++ 15.2.0, CMake 4.2.3, libpng 1.6.57 and libjpeg 2.1.5. It selected `Bm.PQI.Build_00` (`data\PQI\Build_00.jpg`): source 39,671 B; decoded 343x394, 2 BPP, pitch 688, resident 271,072 B, CRC32 `a2870aca`, FNV-1a-64 `04bb5ed285e9e0d8`. Repeat decode matched; all 17 real font metadata resolutions were found with existing backing files.

M13 does not construct Forms, screen objects, script runtime, audio, DirectSound, Steam, registry access, `Rangers::ProgramMain`, or the full global UI initializer.  M14 remains unstarted.