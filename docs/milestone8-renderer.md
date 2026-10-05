# Milestone 8 — software RGB565 renderer

Release oracle: `SpaceRangersHD_decomp` build `2025-10-13`, commit `730bdf6`;
translated C++ oracle `57fa689c630193a66fdea6ca4c79a188814991cd`; OKGF
oracle `c01aa7a168a6f1772541501074b7bba1b96550ef`.

## Rendering boundary

The M8 path deliberately does not emulate Direct3D9:

```text
game drawing code -> real TGraphBufGR CPU RGB565 pixels -> OKGF -> renderer_platform -> SDL2
```

`GR_Main.cpp` is unchanged in the upstream checkout.  The Switch Makefile applies
and removes `platform/gr_main_portable.patch` for its compilation only.  The
patch retains the Windows path verbatim, while the portable path sends
`GR_DXInit` to the software initializer and `PresentScreenBuffer` to the
presentation adapter.

The release `GR_DXInit` has four relevant stages:

1. settings: `Window`, VSync, hardware-render and viewport/frame-limit flags;
2. D3D-specific device/caps/display mode and render-state setup;
3. common software state: RGB565 format, three graph buffers, OKGF callbacks,
   circle/alpha tables and 1024 pending points;
4. presentation: nesting/frame pacing and `PresentScreenBuffer`.

M8 replaces stages 1 and 3 with documented defaults (`1280x720`, 60 Hz,
minimap 156), skips stage 2 entirely, and retains the real stage-4 functions.
No full settings runtime is implied by those defaults.

## Canonical framebuffer and presentation

`ScreenRenderBuffer` is a real `GR_GraphBuf::TGraphBufGR` constructed with
`UseTexture = false`, so it owns a CPU allocation.  Its canonical format is
little-endian RGB565: masks `f800/07e0/001f`, alpha mask zero, two bytes per
pixel. `RenderScratchBuffer` and `AuxRenderBuffer` are real CPU graph buffers.

The adapter owns SDL renderer/texture resources only on Switch. It uploads the
canonical pixels to an `SDL_PIXELFORMAT_RGB565` streaming texture, preserves
aspect through SDL's destination scaling, and never stores an SDL pointer in a
legacy 32-bit graphics field. The host backend is intentionally headless: it
validates dimensions/overflow, hashes the submitted RGB565 bytes and counts
presents. `OKGF_Convert565toBGRA` is also routed to direct linking for an
explicit future fallback, but is not needed when SDL accepts RGB565.

## Direct OKGF bridge

The reached translated wrappers call `platform/okgf_bridge` rather than
`load_import("okgf.dll", ...)` in portable builds. They link the existing
`libokgf.a` directly:

- `OKGR_Fill_WORD`, `OKGR_Copy_XY_XY_WORD`;
- `OKGR_PixelAlpha_16`;
- `OKGF_LineIp_16`, `OKGF_Triangle_16`;
- `OKGF_Convert565toBGRA`.

The bridge has static layout checks for every `WindowsSdk::TRect` / `OkgfRect`
field and converts a non-null clip rectangle explicitly, rather than relying
on a reinterpret cast.

## Regression contract

`tests/test_renderer_software.cpp` initializes a real 64x64 renderer, clears
it through the translated fill wrapper, draws horizontal/vertical graph-buffer
lines, invokes the real line and triangle callbacks, alpha blends a pixel, and
uses the real nested `BeginFramePresentation` / `EndFramePresentation` sequence
to reach `PresentScreenBuffer`.

The initial golden value is FNV-1a-64 `3a9dfdc6db5aacd7`; control pixels verify
the background and primitive writes. The test also checks invalid dimensions,
presentation before init, init/shutdown/re-init and global cleanup.

The next reached boundary after M8 is release-compatible settings/runtime
initialization for actual game-loop startup. Direct3D texture management,
audio, registry, Steam and `ProgramMain` remain outside the reached path.
