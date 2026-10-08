# M24 — Portable GraphButton / Window / Zone Control Foundation

Baseline: `30a255734165ea9eb5def830ad7446bf68f4882c`.
Branch: `codex/m24-ui-controls` in a separate worktree. The original checkout,
the M23 worktree, and the installed Windows game were not changed.

## Portable boundary

`UiGraphButton` owns seven optional `UiImageLeaf` state slots and a prepared
`UiLabelLeaf` caption. It selects Normal, NormalA, Down, DownA, Disable, or
DisableA from explicit Hover, Down, and Disabled state, with upstream fallback
order. Rect, Graph, and ImageHit are separate hit modes. Graph inspects all
configured alpha state images, including inactive ones. Caption state colors,
shadow colors, offsets, localized text, font resolution, state image offsets,
and Auto geometry use the M21/M23 CPU objects. Sound names and `OnPressCode`
remain metadata; no callback or audio system is invoked.

`UiWindow` remains a Panel. It owns nine border images and lays out corners,
edges, and texture with natural tile sizes, MinSize, WorkSubRect, tile alignment,
zero-tile rejection, and child clipping. Its border images have the upstream
internal depth; configured child controls use the normal M22 factory path.
`UiZone` implements Rect and Circle CPU hit tests. Circle retains the upstream
integer-centre and size rule, including its Active/HitTestDisabled quirk.

The generic image path accepts only `Simple`, `Trans`, and `Alpha`; an unprefixed
key means `Simple`. `GI` is not silently substituted. The new production
objects have no Forms, message loop, Direct3D, GraphBuf, script, or audio
dependency. M22 and M23 checkpoints remain separate stages in the cumulative
runtime.

## Read-only release inventory

The occurrence-aware probe reads the installed release's `CFG/Main.dat`,
`CacheData.dat`, language data, and package directories without changing them.
Its machine-readable result is [m24-release-inventory.json](../m24-release-inventory.json).
M23 blocker occurrences counted the same physical control in overlapping
candidate subtrees; they are not physical config counts.

| Control | Physical instances | M23 overlapping blocker occurrences | M24 eligible | Main reason |
| --- | ---: | ---: | ---: | --- |
| GraphButton | 619 | 1,682 | 0 | All 2,035 configured state images use unsupported generic `GI` mode |
| Window | 42 | 68 | 0 | All 378 border images use unsupported generic `GI` mode |
| Zone | 61 | 255 | 61 | 46 Circle, 15 Rect |

GraphButton has Kind Normal 224, Fix 216, Disable 175, FixDisable 4;
KindHit Graph 339 and Rect 280. Its five used image slots are Normal 524,
NormalA 607, Down 603, DownA 121, Disable 180. There are 80 captions,
91 fonts, 77 localized captions, 53 Auto, 28 Down, one Disable, 165
UpOnlyDown, and 601 instances with each sound property. No `OnPressCode`
or configured child control occurs. Of the image references, 2,032 resolve to
package entries and three do not; resource resolution alone does not make the
unsupported `GI` mode eligible. `Help`, `MVUpdate`, `MouseBlocking`,
`MouseBlockingTest`, and `Enabled` also occur outside the M24 subset.

All 42 Windows use nine image slots and WorkSubRect. Eighteen use MinSize;
they contain 186 configured child controls. Every border reference resolves
to a package entry, but the mode remains `GI`. Twelve have `MVUpdate`, and
ten have `MouseBlocking`, both outside the M24 subset. The 61 Zones use no
style, have no unknown Kind, and are all eligible.

There are 599 unique control paths. M23 recognized 377 by type, but only 226
pass this probe's per-occurrence resource/property eligibility. M24 adds five
eligible Zone paths, giving 231 eligible unique paths. No tested hypothetical
support set (`M23`, each new control alone, GraphButton+Window,
GraphButton+Zone, or all M24) yields a real subtree with at least two levels,
three visual leaves, and all required resources. The real candidate is
formally **NOT FOUND**; no real GraphButton or Window oracle is claimed.

Remaining unsupported control-type blockers by candidate subtrees are GAI 74,
GraphBuf 64, PanelScrollBar 43, and Edit 24. Separately, otherwise supported
types fail instance eligibility: Image 244, GraphButton 153, Label 67, Window
55 candidate subtrees. These are overlapping subtree counts, not additive
unlocks. GAI is a candidate for M25 selection, but M25 has not started.

## Synthetic oracle and host validation

`tests/probe_m24_controls.py` independently computes the little-endian
structure and RGB565 frame hashes. The host C++ scene contains a nine-image
Window, two GraphButtons, a Circle Zone, Label, and animated GI leaf. Its
package-backed fixture and the in-memory runtime checkpoint both match the
Python oracle. The three states are A (normal), B (hover), and C (down); the
runtime also cycles through Disabled. The fixed runtime stage verifies A
before starting that dynamic cycle.

| Oracle | CRC32 | FNV64 |
| --- | --- | --- |
| Window layout | `80b94d87` | `77ccd5f317a573d6` |
| Rect Zone hits | `9dd285e1` | `20ffbea85aa4e660` |
| Circle Zone hits | `8ae2e69e` | `d34c2faf3f348e0f` |
| Tree A | `6f65eea5` | `11b73bf4b18ab6fb` |
| GraphButton A | `6bff73cb` | `e1747b752476d6d6` |
| Frame A | `015d1589` | `5523d509a3a9384d` |
| Tree B | `7110370f` | `96c53d4655468267` |
| GraphButton B | `4eb9fddb` | `26b55b559b3eabb9` |
| Frame B | `680b03b6` | `e0b970ea3c332ce8` |
| Tree C | `60f9638e` | `2e2801971a2b76c1` |
| GraphButton C | `ccacf19a` | `3803a382dc99778d` |
| Frame C | `ede9bc02` | `1a306cb1aaa33080` |

Local `host-m24-ui-regression` passed with the retained M22/M23 tests, config
factory checks, state/hit/layout cases, and the independent synthetic oracle.
Terminal GitHub Actions [`37823081354`](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/runs/37823081354)
completed `success` on production commit
`00808c188649a39ece82ad66f17a7e61fcd872f7`. It passed the retained
M12–M23 suite, M24 regression, independent Python oracle, release-probe
self-test, and portable-object symbol audit.

## Software and hardware provenance

The M23 historical RC remains 7,641,392 bytes, SHA-256
`877F15D0B6187B033490815FBBC21229983AA9040FAE27B2F395995C528A2792`,
embedded `build_git=19eb6a5`; it was not rebuilt. The cumulative M24 NRO
retains the M23 source/structure/text/tree/frame expectations and adds the
fixed M24 Window/Zone/tree/GraphButton/frame hashes above.

After terminal CI PASS, a clean local ARM64 build produced an ELF64 AArch64
position-independent executable with **zero undefined symbols**. All five
new M24 production objects (`ui_graph_button`, `ui_window`, `ui_zone`,
`ui_controls_fingerprint`, `ui_controls_checkpoint`) passed the host and ARM64
forbidden-symbol audits. The hardware-ready cumulative NRO is
`Space Rangers HD - A War Apart.nro`, 7,698,736 bytes, SHA-256
`93E7A6AA4B4EE71A9C1F6AE93F72B75F63E193152D61F944E4DA2947187B3C89`,
with embedded `build_git=00808c1`. The final documentation-only commit does
not alter that NRO's runtime inputs. Switch was not run. M24 is **SOFTWARE
COMPLETE / HARDWARE PENDING**, not COMPLETE.

The full mouse dispatcher, focus, keyboard, callbacks, `OnPressCode`, button
audio, GAI/GraphBuf controls, PanelScrollBar, ScrollBar, Edit, Forms lifecycle,
and gameplay remain outside M24.
