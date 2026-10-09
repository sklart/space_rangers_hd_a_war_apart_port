# M26 — GraphBuf, scroll controls, Edit, and larger real UI

## Evidence boundary

M26 starts from M25 `ac2e0063ac1204ef70b579ebba3b50eacb922d38` on
`codex/m26-ui-next`. The release probes read the local game's `Main.dat`,
`CacheData.dat`, packages and fonts. Only metadata, hashes, and source code
are committed. Host tests and Python oracles establish software behavior;
the cumulative NRO still requires a physical Switch run and screenshot.
The final production source is `8c092352f262f54231570e2811f08b61f555726a`.
Its clean ARM64 NRO embeds `8c09235`, is 7,833,904 bytes and has SHA-256
`ACAAB6C2FDAB8BCD9CED46F5462CA2FE57971AB8212054576ADB315133A152CB`.
The ELF is AArch64 with no undefined or forbidden M26 symbols.

## Portable controls

`GraphBufferCpu` owns bounded RGB565 or BGRA pixels, supports allocation,
clear, aspect-fit, CacheRGBA through the M21 bitmap path, and CacheGI through
the M25 GI path. CacheRGBA uses bilinear sampling; CacheGI uses the OKGF
filter-5 Lanczos3 path. The latter has matching native-host and ARM-compatible
host output, rather than a separately reimplemented Python resampler.
Rendering supports M21 layout modes, per-pixel alpha, RGB565 HalfAlpha,
pixel hit and visual centre. Extreme coordinates and excessive tiling fail
before plan construction. Borrowed external buffers and screen capture remain
deferred.

The release contains **68 physical GraphBuf**, all with dynamic sources:
zero have config-backed CacheGI or CacheRGBA and zero meet render eligibility
from the release config alone. The runtime diagnostic instead feeds a real
`DATA/FormLoad2/2BarCenter.gi` resource into a portable GraphBuf: source
2,976 bytes, CRC32/FNV64 `19267238/debceda99798e0e0`, decoded 29×37,
aspect-fit 39×50 within 100×50. Scaled BGRA is
`07bfadc2/1dc1b91c9c87ca5a` (7,800 bytes); the isolated 1280×720 frame is
`1ebb3fc8/eb791740957eeeb8`. The fixed diagnostic hit succeeds and the
visual centre is `(49,24)` in local coordinates. Source/decode fingerprints
come from the independent package/GI oracle; the scaled and frame hashes
are host/ARM-compatible regressions.

`UiScrollBar` implements the seven image regions with normal/active/down
states, horizontal/vertical geometry, both calculation formulas, range
clamps, narrow-track behavior, hit regions, and programmatic state changes.
It preserves upstream `SetLargeChange`'s comparison with `SmallChange`.
All **3 physical ScrollBar** controls are horizontal and render eligible;
one is active. The chosen real `PF_SBTurn` style has 51 GI image references
across all physical bars. Its independent layout oracle fixes track 209,
thumb 56, before 0, after 153 and layout CRC32/FNV64
`6bb5529f/835228303672b5fb`. The isolated real scroll frame is
`e28dc3bc/ae41399ead94b351`.

`UiPanelScrollBar` owns internal bars, positions external bars in the parent
coordinate system, derives scroll ranges from active world children and
synchronizes offsets in both directions. **27 physical** instances exist;
**22** meet the conservative render subset. The selected Achievements
`PanelSlot` uses an external vertical bar at release rect
`(903,116,923,629)`, size 20×513. Its empty-world safe range is 0..539,
page 540, position 0; the independent 40-byte layout record is
`3334a660/82cd5b1de32b0ba2`. Dynamic removal/reparenting and timer autorepeat are
outside the current static showcase path.

`UiEdit` adds AFT-backed text, optional bitmap background and border, left
or centred alignment, deterministic caret/focus/blink, max length, and pure
insert/delete/navigation methods. **49 physical Edit** controls use the
supported `Font.2Normal` visual path and are render eligible; keyboard input,
callbacks and focus dispatch remain deferred. The selected
`Info/PanelM11/Edit#0` renders `123` at `(589,344)`, with a focused 2×14
caret at `(615,346)`. Its independent focused frame is
`0c547e1b/18432c76a6d566c5`. The isolated unfocused and focused
caret-off frames are both `52c44a22/490042511484a325`: the release Edit
uses black text on the zero diagnostic framebuffer, so the visible difference
in this isolated oracle is the red caret. The full release panel frame above
checks the actual surrounding imagery.

## Largest real release subtree

Overlap-aware search counts 90 at M25, then 90 after GraphBuf, 91 after
ScrollBar, 98 after PanelScrollBar, and **114** after Edit. These are actual
unique subtree counts, not a sum of overlapping blocker estimates. The
lexicographically smallest remains the M25 18-node PLBar; the most diverse
candidate is also the largest M26 candidate. The largest
fitting candidate is `ML#0/Info#0/Panel#0/Panel#4/Panel#11`: **50 nodes**,
depth 2, **49 visual leaves** (3 Edit, 28 GraphButton, 1 Image, 17 Label),
up from M25's largest 40 nodes/depth 3/27 leaves and selected 18-node PLBar.
It retains release placement `(298,120)` and size 410×435 on the 1280×720
screen. This is about 19% of the frame area; enlarging it to the requested
80–90% would change release geometry, so the diagnostic preserves its true
size. The release tab is initially inactive; the diagnostic explicitly
activates it for display. Tree CRC32/FNV64 is
`67b1fcbf/2940a0ef334dd86e`. An independent Python Main.dat/package/AFT
renderer and the C++ host renderer agree on the full 1,843,200-byte RGB565
frame: `cb1a12b3/fbca196e86b2f452`, with zero pixel differences.
The separate diagnostic zone shows real-resource GraphBuf, ScrollBar,
PanelScrollBar and Edit state without changing this subtree's release
geometry. Runtime logs decoded-byte estimates and update/render timing.
The release-backed host estimate reports GraphBuf 7,800 bytes, scrollbar
images 34,704 bytes, four font sources 96,506 bytes, subtree decoded data
1,113,546 bytes, and peak UI estimate 4,842,450 bytes including two
1280×720 framebuffers. These are accounting estimates, not a Switch heap
high-water measurement. Runtime also logs its own estimate and per-frame
update/render average and maximum times.

The leading remaining render blocker roots overlap: Image 104, dynamic
GraphBuf 68, GAI 57, PanelScrollBar 10, GraphButton 8, ShrLight 7,
StarField 6 and MultiImage 5. Interaction remains a separate blocker for
input/focus/event routing. M27 should first examine that foundation and the
dynamic Image/GraphBuf sources; neither is implicitly solved by M26.

The requested Image-only audit finds 45 roots at the M25 baseline: 34 have
an empty effective image key (dynamic or missing), and 18 have a resolved GI
key whose format or render semantics are not in the supported subset. These
class counts overlap. At M26 the corresponding unique count is 47, with 36
empty-key and 19 GI-category roots; other controls becoming eligible exposes
two more Image-only roots. No unresolved Simple, generic GAI, animation or
GraphBuf image mode was observed in these Image-only roots. This is a
classification of blockers, not a fallback that draws unknown content.

## Verification and next hardware gate

The M26 host unit targets cover GraphBuf, ScrollBar, PanelScrollBar and Edit.
Release-dependent C++ tests require `GAME_ROOT`; release probes require the
local licensed assets. CI runs synthetic targets, probe syntax/self tests,
retained M12–M25 gates and forbidden-symbol audit without game art. The NRO
must preserve M23/M24/M25 checkpoints before M26 and report the exact
`build_git`, all M26 PASS markers, frames/presents, perf/memory lines,
`PLUS`, shutdown and `[BOOT] COMPLETE`. Keep it running at least 45–60 s,
capture `port.log`, `gr-main.log` and a screenshot, and compare the SD copy
SHA-256 when accessible. Until that run, status is **M26 SOFTWARE COMPLETE /
HARDWARE PENDING** only after CI and clean ARM64 validation are recorded.
