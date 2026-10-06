# M13 GI feasibility boundary report

This is analysis only; it does not add `EC_CacheGI.cpp` to the Switch build and does not begin M14.

`TCGiEC::LoadFromConfigBuffer` is CPU-capable in principle: it performs `ApplyWideScreenLayoutFixups`, then `TgiGR::LoadRawGiFromBuffer`, and retains the raw GI byte count. The fixup path itself can decode to `TGraphBufGR` and re-encode GI data, so it is not intrinsically Direct3D-bound.

The Direct3D boundary begins at `TCGiEC::GetOrCreateSurface`: it creates/uses `SurfaceCache`, calls `GR_DX::GR_CreateTexture`, locks `IDirect3DTexture9` surfaces, and feeds pixels through `DecodeRawRegion` or `DecodeToPixels`. Texture caching and tiled-surface management therefore must stay out of a future CPU-only probe.

A possible M14 experiment is technically feasible only as a separately authorized CPU path: `GlobalCache::OpenDataBuffer` → isolated `TCGiEC::LoadFromConfigBuffer`/`TgiGR` parse → dimensions/content/hash report → destruction. It must first demonstrate that widescreen fixups are disabled or expected for the selected key, and must not call `GetOrCreateSurface`, construct Direct3D resources, or alter cache lifecycle. This report is not implementation approval.