# Milestone 13 UI boundary

M13 is a CPU-only, non-interactive slice.  It must not initialize Forms, screen objects, scripts, audio, a cache worker, `Globals::InitializeGlobalUiRuntime`, or `Rangers::ProgramMain`.

| Area | M13 action | Ownership / boundary |
|---|---|---|
| Font metadata | Sets and records the 17 release `Font.*` names | Slice owns only names and its resolution vector |
| Font smoothing | Reads `FontSmooth` from `UserSettingsConfig` | `SetCacheKey` keeps upstream alias semantics |
| Cache roots | Reads `CacheDataRoot`; opens direct data buffers | M13 never frees or replaces roots/cache/config |
| Bitmap decode | `OpenDataBuffer` → `TCBitmapEC::LoadFromConfigBuffer` | CPU pixels only; no cache worker/control acquisition |
| Decoder failure | Rejects invalid/oversized images before allocation | caller dimensions remain zero; codec context is cancelled |
| Renderer | Uses the existing software renderer only for the bitmap format contract | no D3D surfaces or presentation-loop change |

The synthetic guard covers empty, truncated, invalid and `0x7fffffff × 0x7fffffff` BMP input.  Each failure leaves bitmap state clean.  Valid fixture semantics remain default `2/4/8`, RGBA `4/8/16`, RGB `3/6/12`, Gray `1/4/8` (`bpp/pitch/resident`), with deterministic repeat decode.

Status: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE NOT REQUIRED**. The release probe passed on Ubuntu 26.04 WSL (GCC/G++ 15.2.0, CMake 4.2.3, libpng 1.6.57, libjpeg 2.1.5). It selected `Bm.PQI.Build_00` / `data\PQI\Build_00.jpg`: source 39,671 B; decoded 343x394, 2 BPP, pitch 688, resident 271,072 B, CRC32 `a2870aca`, FNV-1a-64 `04bb5ed285e9e0d8`; repeat decode matched. All 17 release font resolutions were found and their backing files existed.

M12 remains **HARDWARE PENDING**.  M14 is not part of this milestone.
## M13 completion matrix

| Slice | Status | Evidence/boundary |
|---|---|---|
| A. Scalar/user settings | PASS (M12) | portable CFG and derived runtime state |
| B. Font/resource metadata | PASS | 17 fixed keys, `FontSmooth` true/false and alias regression |
| C. Cache loader/thread | DEFERRED | `AcquireDataFromConfig` is intentionally not called |
| D1. Bitmap cache | HOST PASS | bounded direct CPU decode and real-release probe with deterministic repeat fingerprint |
| D2. Alpha/TBitmap | DEFERRED | not entered |
| D3. GI | ANALYZED / DEFERRED | CPU boundary documented; no implementation |
| D4. GAI | DEFERRED | not entered |
| E. Script runtime | DEFERRED | not entered |
| F. Screen registration | DEFERRED | not entered |
| G. Real first screen | DEFERRED | not entered |

The local-only `host-bitmap-cache-release-test` accepts the original game root and a disposable user root. It prints all 17 `CacheData.dat` font resolutions and selects the first decodable real file deterministically (font-related keys first, then UI/panel keys, then lexical key order). It records source and decoded fingerprints without writing game assets or pixels. It is deliberately excluded from asset-free CI.
## GI boundary (analysis only)

`EC_CacheGI::TCGiEC::LoadFromConfigBuffer` and `GR_gi::TgiGR::DecodeToGraphBuf` are CPU-side candidates for a separately authorized M14 experiment; widescreen fixups are also CPU work. `TCGiEC::GetOrCreateSurface`, `SurfaceCache` and the Direct3D texture path are excluded. The detailed analysis is in `docs/milestone13-gi-boundary.md`; this does not implement M14.
