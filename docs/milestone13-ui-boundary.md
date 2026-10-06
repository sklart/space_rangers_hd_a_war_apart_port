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

Status: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE NOT REQUIRED**.

M12 remains **HARDWARE PENDING**.  M14 is not part of this milestone.