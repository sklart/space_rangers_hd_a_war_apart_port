# M25 — portable GI/GAI UI and first release subtree

## Scope and evidence

M25 starts at `6193878a71341c9726a0935643c0170305032c6b` after the M24
fast-forward to `master` and the `m24-hardware-pass` tag. The release probes
read `CFG/Main.dat`, `CFG/CacheData.dat` and package payloads without writing
game content. The JSON inventories in the repository contain metadata and
hashes, never extracted art. The C++ release tests require a local, licensed
installation via `GAME_ROOT`; CI has no release assets and runs synthetic
gates plus probe syntax checks.

## Raw GI

The complete physical GI-reference inventory contains 1,192 distinct keys:
1,157 Format 2, 22 Format 0, six other formats (Format 1/3) and seven
unresolved keys. The largest raw source is 13,440,096 bytes; the largest
decoded image is 26,880,000 bytes. `GiImage` uses the M14P/M16 decoders,
validates the header, dimensions, bounds, plane spans and 256 MiB source
limit, and retains decoded BGRA pixels with the actual GI origin. Layout,
global alpha, black-pixel hit testing and visual centre use the M21/M18
CPU paths. `HalfAlpha` does not apply to raw GI. Unsupported formats fail
closed. Synthetic corruption and origin/alpha tests run in
`host-gi-image-test`.

At 1280×720, 19 referenced GI keys match the audited upstream widescreen
fixup families. The three resources in the selected subtree match none of
them; the other transformations remain deferred. See
`m25-widescreen-inventory.json` for conditions and individual keys.

## GAI

There are 178 physical GAI controls: 136 resolve an Image, 98 declare PBuf,
76 declare ImageFirst, and 56 resolved instances declare neither. Two of
those 56 contain Format 3/4 frames, leaving 54 conservative render candidates.
Another 42 physical controls have no Image in the release config and may be
assigned one dynamically. The largest GAI source is 30,750,866 bytes.
Eighty resolved instances use cumulative flag 1; two shared resources among
them have non-standalone delta frames. Those frames are classified as
`cumulative_delta`, not corrupt standalone GI, and remain unsupported by M25.
The largest extracted frame source is 609,817 bytes; the largest frame BGRA
capacity inferred from bounds is 1,905,244 bytes. The largest PBuf candidate
canvas would also require 1,905,244 BGRA bytes. These are inventory limits,
not a claim that PBuf is implemented.

`UiGaiLeaf` uses M15/M16 decoding and M17 playback with deterministic tree
delta. It supports embedded `FrameLoad`, bounded custom `Frame` ranges,
frame positioning, Stop, StopAfterOneCycle, per-frame bounds, layout, global
alpha and pixel hit testing. Generic `Image=GAI,...` chooses sequence zero.
PBuf cumulative composition and its ImageFirst layer are not implemented;
these instances are unsupported even where individual frames can be decoded.
Audio for SoundStart and update-rectangle optimization for
SkipImageUpdateRect are deferred. The 54 candidates are **render-only**:
input/event routing is outside M25. Format 3/4 and unresolved resources are
unsupported. The real `Bm.PI.PathEndMove` GAI oracle proves frame 0 and the
next frame after 70 ms, including frame and RGB565 canvas hashes.

## Base controls and eligibility

The base object retains Help text, MVUpdate, MouseBlocking and
MouseBlockingTest metadata. Help resolves through language data when supplied;
help callbacks, mouse occlusion and mouse-view invalidation remain deferred.
One `Enabled=False` occurs at
`ML#0/GameSettings#0/Panel#0/Panel#1/Panel#0/Panel#0/GraphButton#2`.
The upstream GraphButton loader does not read that field, so the occurrence is
not promoted to render eligibility without a stronger semantic proof.

After raw GI, 89 candidate subtrees meet the conservative structural and
visual test; after supported GAI, 90. The selected lexicographically smallest
path is
`ML#0/AB#0/Panel#0/Panel#0/Panel#1/Panel#0/Panel#0` (`PLBar`):
18 nodes, depth two, 17 real Image leaves and three distinct Format 2 GI
assets (`Bm.FormLoad2.2BarLeft`, `2BarCenter`, `2BarRight`). Its local canvas
is 321×37. The independent Python Main.dat/package oracle fixes tree
CRC32/FNV64 `73a25b4c/26d6a269e96b959b` and RGB565 frame
`9cec8dc2/39c2ccfd0deb5fbb`. The C++ release test reads Main.dat and
CacheData.dat through portable read-only loaders, builds children through
`ui_config::LoadChildren`, resolves GI keys with the same
`CacheUiResourceResolver` used on Switch, and checks those same hashes. It
also resolves real `Bm.PI.PathEndMove` GAI by CacheData key and checks its
first rendered frame.
The local-canvas normalization changes the panel's release position; the
fingerprint is a subtree proof, not a screenshot of the full game screen.

The largest candidate has 40 nodes, depth three and 27 visual leaves at
`ML#0/Ship#0/Panel#0/Panel#1/Panel#2/Panel#1/Panel#0`.
Eligible physical counts under the conservative model are GraphButton
614/619, Window 42/42, GAI 54/178, Image 995, Label 700, Panel 375 and
Zone 61. Interactive eligibility is not claimed. Root blocker counts in
`m25-eligibility.json` overlap by ancestry and must not be summed.
The same report contains one record for each of the 3,375 physical controls
at both stages, with separate type, resource, render and interaction axes,
category and reasons. After GI, 2,787 controls are `RENDER_ONLY` and 588
are `UNSUPPORTED`; after GAI, these become 2,841 and 534. No control is
marked `FULL` while the mouse/event dispatcher is deferred.

The release GraphButton candidate uses real GI Normal, Hover, Down and
Disable state resources; its normal/hover/down framebuffer CRC32/FNV64 are
`1c585c8f/7f23dbb7c1d8df15`, `1a6eae32/46793341f42dd912` and
`663cb736/c45ecedf7f592470`. The release Window candidate is inactive in
Main.dat; its separately forced-active nine-GI border oracle is
`87e5a68f/8020187c43bbcc26`. The release-backed factory test additionally
resolves nested `Style.GB.SoundNormal` and `Style.Window.2Simple` paths,
constructs `F1` and the isolated `InfoPanel` border through CacheData keys,
and matches those same independent frame hashes. It reads real
`MVUpdate=True`, `MouseBlocking=True`, `MouseBlockingTest=False` on `F1`,
`MouseBlocking=False` on `InfoPanel`, and localized `Help.ButAI` on `ButAuto`;
removing Help from a copied config leaves the rendered frame unchanged.
The original InfoPanel remains inactive and its GraphBuf child remains
unsupported; the forced-active border proof does not claim original-scene
appearance.

## Validation boundary

`host-m25-ui-regression` retains M12–M24 host gates and adds synthetic GI
and GAI tests. Local release tests are `host-m25-real-ui-test`,
`host-ui-gai-release-test` and `host-m25-controls-release-test`. The M25 CI
stage compiles production objects and audits forbidden original GI/GAI,
Direct3D, Forms and SoundManager symbols. A clean ARM64 build and physical
Switch M23/M24/M25 cumulative checkpoint require separate evidence; host and
CI results alone do not grant hardware PASS.
The local retained host run and all three release tests pass. CI run
`37851721838` passed for production commit `e2334a4`, including the M25
portable object symbol audit. The subsequent clean ARM64 build produced an
ELF64 AArch64 executable with zero undefined symbols and an NRO of 7,727,408
bytes, SHA-256
`F9D1E7A5C91DFA24433A0BD1AEB69009BD393CD864EE21C4FB177701D46989CC`,
embedded `build_git=e2334a4`; the six new M25 objects have zero forbidden
symbol matches. The M25 runtime stage additionally requires an observed GAI
source-frame transition during the live UI tree update; the pre-loop frame-1
oracle alone cannot satisfy that gate. A physical Switch run, SD transfer
proof, and screenshot are still pending. The synthetic deployment regression
also passes with a read-only licensed Rangers.exe
fixture and an isolated fake SD root; it checks the manifest and equal NRO
source/destination SHA-256. No physical SD transfer is inferred from this test.

The next subtree unlock should be chosen from the measured blockers. Current
roots include GraphBuf 65, PanelScrollBar 43, Edit 24 and Image 104; Image
must first be split by actual unsupported mode/resource before selecting M26.
An upper-bound what-if count over the recorded 267 structural panel/window
roots finds 45 roots blocked only by `Image`, 19 only by `GAI`, 16 only by
`Edit`, nine only by `GraphBuf`, and seven only by `PanelScrollBar`.
The 45 `Image` roots involve unresolved/dynamic Simple or GI resources, not
one missing image mode; GAI includes cumulative PBuf, while Edit requires the
deferred input layer. GraphBuf remains the tentative M26 recommendation as a
single reusable missing control type, but its nine-root figure is only a
best-case upper bound and its CPU implementation cost is not yet measured.
M26 has not been started.
