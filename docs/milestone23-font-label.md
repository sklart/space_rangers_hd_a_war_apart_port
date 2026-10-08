# M23 — Portable AFT Font / Tagged Text / Label Foundation

Baseline: `022aafdfdab0f022fee043aac0ca3291637d6832` (M22 hardware PASS).
All M23 work lives in the separate `codex/m23-font-label` worktree. The legacy
M14 checkout and the installed game are read only.

## Portable boundary

The AFT parser validates the 32-byte header, 64-byte glyph records, UTF-16 BMP
codes, dimensions, offsets, sizes, and both OKGF RLE planes before publishing
a font. The parser keeps a BMP lookup table and computes upstream baseline,
line and advance metrics. Opaque planes write the current RGB565 color; alpha
planes use the existing OKGF 16-bit blend path. It creates no `TCFontEC`,
`TCFontControlEC`, Direct3D, system font, or font worker.

Tagged text keeps per-layout color, fixed-width, format, tab, align, and object
state. Object IDs, size, vertical mode and layout positions are available;
creation and callback materialization of arbitrary embedded UI controls are
deferred. The `UiLabelLeaf` uses cached parsed AFT and prepared line layouts,
then draws background image, shadow, four diagonal outline copies, main text
and outer border in that order. Ordinary rendering does not re-read AFT or
allocate a framebuffer-sized image. The synchronous `Repository` owns one
immutable parsed font per cache key and has no worker, eviction, or LRU policy.

`FontSmooth` alias switching uses the four existing M13 `GlobalsV` plain/smooth
pairs. Other font names, including `Font.2Intro`, are left unchanged. The M22
factory now accepts `Label`, repeats `Text` parameters in order, and resolves a
localized key only when it exists. The M23 runtime finds one release Label by
its occurrence-aware config path and loads it through that same factory.

## Software validation

The synthetic fixture contains space, `A`, `B`, `C`, `Ё`, and `я`, including
opaque-only, alpha-only, mixed and negative-offset glyphs. Host tests exercise
corrupt headers/tables/planes/RLE, RGB565 drawing, tags, measurement, wrapping,
justification, Label properties, config/style/localization, clipping and the
mixed M20/M21/M22/M23 UI scene. The mixed scene has two frames: text, panel
scroll and GI animation change between frames. Its oracle is independently
calculated in `tests/probe_m23_ui_text.py`.

| Synthetic fingerprint | CRC32 | FNV64 |
| --- | --- | --- |
| AFT source | `6b655a7f` | `f7cedbbab2168e77` |
| AFT structure | `2f1c1442` | `faff5b46e2b3d02e` |
| Layout A | `00af5933` | `6fc00b179d894698` |
| Tree A | `19bf1b94` | `3a3e48a85aedf03b` |
| Frame A | `e227128c` | `c27811b5ad7e7fdd` |
| Layout B | `35806d66` | `0b66e23e077a1dd2` |
| Tree B | `adbefa24` | `b446e66bca28b6f4` |
| Frame B | `3793178e` | `38c61f5d7d590181` |

The independent release AFT probe validated **17/17 fonts**, 3,831 glyphs,
zero invalid fonts and zero duplicate codepoints. The largest source is
`Font.Verdana12` (58,185 bytes). The largest encoded glyph plane is 325 bytes
in `Font.2Huge`. Python metrics and structural hashes matched the C++ parser
for all 17 fonts.

The fixed real Label is the lexicographically first eligible release candidate
for the runtime's Russian language:

`ML#0/AB#0/Panel#0/Panel#0/Panel#1/Panel#4/Panel#0/Label#0` (`WinText`).

It uses `Font.2Intro` from `forms.pkg:DATA/FONT/Verdana_11_3.aft`
(26,669 bytes). Source CRC32/FNV64: `93df743f`/`4c320b6bc6048343`;
AFT structural CRC32/FNV64: `7ecfe087`/`1a1527c347b838c2`.
The font has 214 glyphs, line height 16, centering height 11, above/below
baseline 16/2 and maximum advance 16. Localized UTF-16 text fingerprint:
`b64f251b`/`780519c70238915f` (`В ы  п о б е д и л и !`).
Measured bounds are `[1,-12,156,2]`, content size `156×17`, one line,
text X `435`, baseline Y `36`. The independent RGB565 oracle renders a
`1024×60` region with frame CRC32/FNV64 `b36cfe2f`/`6b916c3b29d2a194`.
Its fixed two-node UI tree is `ef3ef436`/`6022c76fb3cb9306`.
The local C++ real-font test matches the independent Python values.

The release `Main.dat` has 599 unique control paths with properties: supported
count rises from M22's 267 to 377; unsupported is 222. The occurrence-aware
scan sees 3,383 control blocks, including 701 Labels (667 direct fonts, 33
inherited fonts, one without a font). Russian resolved Label text has four
opening and four closing color tags and no unknown tags. There are 246
WordWrap, three text-border and 50 text-shadow Labels; no Label has an embedded
Image or outer Border in this release. One Label has no font; other selected
Label texts have glyph coverage. `Help` (65) and `MVUpdate` (9) are properties
outside the M23 Label subset and are excluded from real-baseline eligibility.

No fully supported real subtree meets the required two-level, three-visual-leaf
threshold with all resources resolved. The leading blockers by candidate
subtrees are GraphButton 151 (1,682 occurrences), GAI 90 (454), GraphBuf 51
(102), PanelScrollBar 44 (58), Window 32 (68), Edit 24 (145), and Zone 16
(255). These are inventory data for a separate M24 specification; M24 has not
started.

The runtime M23 stage loads the real font from the current cache mapping,
checks source/structural/metrics/UTF-16 fingerprints, builds `WinText` from
the release config, renders the fixed tree, compares the independent tree and
frame hashes, then overlays the prepared Label during the existing dynamic
M12/M17/M20/M21/M22 loop. The M21/M22 first-frame oracle is checked before
that overlay.

The local M23 host regression, synthetic Python oracle, all 17 release-font
Python/C++ comparisons and the real Russian Label Python/C++ oracle pass.
GitHub Actions `package-host` run
[`37803515979`](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/runs/37803515979)
finished `success` on M23 RC commit
`19eb6a5414e8ca59df3439fe484bb4ce59f0822e`. It passed the retained
M12–M22 regressions, M23 host regression, independent Python oracle, release
probe self-tests and separate M23 portable-object symbol audit.

After terminal CI PASS, a clean local ARM64 build produced an ELF64 AArch64
position-independent executable with **zero undefined symbols**. All six new
M23 production objects (`aft_font`, `font_renderer`, `font_repository`,
`font_cache_resolver`, `tagged_text`, `ui_label`) passed the forbidden-symbol
audit. The hardware-ready RC is `Space Rangers HD - A War Apart.nro`,
7,641,392 bytes, SHA-256
`877F15D0B6187B033490815FBBC21229983AA9040FAE27B2F395995C528A2792`,
with embedded `build_git=19eb6a5`. This exact NRO was not deployed or run on
Switch.

## Hardware validation

**PENDING — Switch unavailable.** M23 is **SOFTWARE COMPLETE / HARDWARE
PENDING**. It must not be marked `COMPLETE` until a future hash-bound M23 or cumulative NRO
matches font/tree/frame checkpoints on physical Switch, continues the dynamic
loop, exits with `PLUS`, and reaches `[BOOT] COMPLETE`. No deployment is part
of this phase.

## Deferred after M23

Buttons, mouse/keyboard/focus/hover, Forms/Window/Edit/ScrollBar/CheckBox/
RadioGroup, scripts/events, full embedded object callbacks, hardware texture
Label path, audio and gameplay remain outside this milestone. There is no
release Label `ImageHalfAlpha` instance; that mode is not inferred.
