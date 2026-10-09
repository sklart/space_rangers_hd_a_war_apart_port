# M27 — portable input, focus and event dispatch

Baseline: `3715e522e6b67a78d70afdb3c112552914bb9c26` (M26). The original M26 RC was 7,833,904 bytes with SHA-256 `ACAAB6C2FDAB8BCD9CED46F5462CA2FE57971AB8212054576ADB315133A152CB` and `build_git=8c09235`; the later cumulative M27 hardware run also validates M26.

M27 status: **COMPLETE / HARDWARE PASS**. The initial production source commit was `f373d33cf8251bc995e12331e253f0003c2b34d1` on `codex/m27-input-focus`. [GitHub Actions run 37892884561](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/runs/37892884561) completed successfully on that exact commit, including retained M12–M26 checks, M27 input tests, and the portable-symbol audit. A subsequent clean devkitA64 build produced an ELF64 AArch64 executable with zero undefined symbols and zero forbidden symbols in the four M27 core/adapter objects. The original 7,866,672-byte NRO had SHA-256 `5CAC745971CAFD9F69CBDB664CDB1914005BEB8B6E583CFDF242DB9A536BD845` and `build_git=f373d33`; the successful physical runs use the corrected `f9c98de` NRO documented below.

## Scope and evidence boundaries

| Layer | M27 support | Evidence |
| --- | --- | --- |
| Rendering | M26 real Info and Film control trees, RGB565 compositor | Retained M26 tree/frame hashes and five M27 button-state frames |
| Interaction | Recursive pointer dispatch, mouse-inside, one hover and focus target, occlusion, GraphButton/Zone/ScrollBar/PanelScrollBar/Edit | Host core and real-release tests; fixed interaction oracles |
| Actions and scripts | Ordered portable action queue with path, sequence, payload, old/new state and little-endian CRC32/FNV64 fingerprint; script and sound **requests** only | Python event oracle and C++ comparison; no script or audio execution |
| Switch hardware | libnx PadState input adapter, live virtual pointer, scripted release interaction before manual input | ARM64 and NRO are software evidence; physical run pending |

The router is independent of the Windows `GI_MessageLoop`. It walks every active hit child in tree order, including overlapping children. A separate reverse-order query handles `MouseBlocking` and `MouseBlockingTest`. A tree mutation observer clears focused, hovered and pressed pointers when controls detach, deactivate or are destroyed. A control's `MouseInside` flag is distinct from the single hovered control.

The Switch adapter polls one PadState per frame. Left stick and D-pad move a pointer clamped to the framebuffer; A/B produce left/right transitions and PLUS exits. The runtime order is platform pump, applet check, input poll, PLUS check, input dispatch, frame update, draw, present. Host tests inject raw states into the same normalization function.

## Fixed release oracle

The Info candidate is `ML#0/Info#0/Panel#0/Panel#4/Panel#11/GraphButton#27`, name `M11Clear`, a 67×21 Rect hit target at `(459,537)`. `tests/probe_m27_button_props.py` verifies its release `Main.dat` properties, including normal/hover/down images and three sound keys. `tests/probe_m27_real_interaction.py` decodes release resources independently of the C++ renderer. `m27-real-interaction-oracle.json` fixes the five RGB565 frames:

| State | CRC32 | FNV64 |
| --- | --- | --- |
| Normal and leave | `cb1a12b3` | `fbca196e86b2f452` |
| Hover and button up | `fb83833b` | `5c7d41d26204f9cd` |
| Button down | `12dd6ab5` | `ffe66f7b03cca7cd` |

`tests/probe_m27_event_trace.py` independently checks those release properties, models the upstream traversal order and serializes the action record. The fixed `m27-event-oracle.json` expects 20 actions, 1,327 bytes, CRC32 `004504e6`, FNV64 `1192cd139f06a4a0`. Event metadata inventory in `m27-event-metadata-inventory.json` found no `OnPressCode`, `OnMouseEnterCode`, `OnMouseLeaveCode`, `OnMouseRightClick` or `OnKey` blocks among 4,727 parsed Main.dat nodes. Sound keys are emitted as requests; no sound is played.

`m27-interaction-state-oracle.json` fixes additional host-observed checkpoints before any Switch run: Edit focused caret-on `84415dcd/6ad55cf9f6e3b52a`, text `1` `f4e281dd/f9206d95dea1c1cb`, caret-off after clearing text `cb1a12b3/fbca196e86b2f452`, and Film ScrollBar position 53 `50d0d772/230a5072582bdb54`. The button frame oracle is independently decoded in Python; these extra Edit/ScrollBar frames are fixed from the local C++ host run and have different provenance.

The 2026-10-09 physical run with `build_git=241efc4` reached the M27 scripted drag but stopped on its frame check. The runtime renders the relocated Film ScrollBar together with PanelScrollBar and M26 GraphBuf; the earlier `50d0d772/230a5072582bdb54` host frame contained only the two scroll controls at their original test position. A release-backed host test now checks both compositions. Its runtime composition matches both supplied Switch logs exactly: `c460a53e/c68235dbfa5624b5`. The scripted failure prevented `[M27] LIVE INPUT READY`, so the virtual cursor and manual control dispatch never activated in that run. M27 hardware validation remains pending a rerun.

The subsequent `f9c98de` NRO ran on Switch with local SHA-256 `99F9B88B8FCD224B7D1F10FD35FB24AF67DA359571C0B880EF09D60E8734A098`. The log confirms scripted interaction PASS, live input ready, 16 A presses/releases at changing cursor positions, M17 cycle PASS, 633 frames and presents over 40,744 ms, PLUS exit, every retained stage PASS, shutdown PASS and `[BOOT] COMPLETE`. The user observed and moved the crosshair. B/right-click was not seen in the log; X/Y are not mapped. No SD destination hash or screenshot was supplied, and the run is shorter than the documented 45–60 s smoke interval. This is **hardware partial**, with final smoke pending.

A further physical run of the same `f9c98de` build supplied `G:\port.log`, `G:\gr-main.log` and a 1280×720 screenshot. After `[M27] LIVE INPUT READY`, the log records five B/right-down and five right-up transitions at `(844,520)` and `(821,473)`, plus three A/left-down and three left-up transitions. The screenshot visibly shows the real UI subtree and white pointer. Scripted M27 interaction, all retained runtime stages, shutdown and `[BOOT] COMPLETE` passed; PLUS ended 349 frames/presents after 24,150 ms. This establishes physical B dispatch and visual presentation, but this individual run also falls short of the documented continuous 45–60 s smoke interval. The SD destination hash remains unmeasured. Status: **hardware partial / final duration smoke pending**.

The final `f9c98de` run stayed in the loop for 116,707 ms with 1,960 frames/presents, exceeding the requested observation interval. After live input readiness it logged four A/left and six B/right press/release pairs. M17 cycle and all M17–M27 runtime stages passed; PLUS, shutdown and `[BOOT] COMPLETE` were orderly. Together with the preceding same-build screenshot, this closes the physical M26/M27 smoke gate: **complete / hardware pass**. The SD destination hash remains unmeasured; the local tested NRO SHA-256 is recorded above.

The release Film ScrollBar thumb drag moves position `1→53`; a left-arrow press and repeat at 500 ms plus three 50 ms intervals move it `53→52→51→48`. The real PanelScrollBar's release callback is checked with an explicitly synthetic 1,000-pixel child world, producing offset `0→1→2`; the isolated release panel itself has no scrollable world in this fixture. The release Edit is focused, receives a character, clears via Shift+Backspace and blinks its caret through router events.

`m27-real-occlusion-oracle.json` records a separate release `Info/PM_PanelMsg` control with `MouseBlocking=True`, configured position `(111,741)` and size `362×26`. The host test recreates those configured bounds with a portable blocker and verifies query result `1` ahead of an ignored target, then `-1` for the blocker itself. This proves the query against real configuration geometry without claiming the full message panel or gameplay occlusion was loaded.

`m27-interaction-inventory.json` records render/pointer/focus/keyboard/action/audio/script axes for all 3,375 physical controls and retains coded M25 and M26 categories for comparison. Pointer-ready counts are GraphButton 614/619, Zone 61/61, ScrollBar 3/3, PanelScrollBar 22/27 and Edit 49/49. Within M26's 114 renderable subtrees, 75 contain a pointer-ready control, 17 contain a focus-ready control and one has a complete portable action queue without deferred audio or script requests. GraphButton remains `ACTION_DEFERRED` even when hover and click work: 598 of the 614 pointer-ready instances request sound and no gameplay callback is executed.

## Switch smoke, when hardware is available

Verify the M27 NRO's local SHA-256 before copying it to SD; rehash the destination if possible. Start the game with the original licensed assets, wait for the cumulative M23–M27 fixed and scripted PASS markers, then move the virtual pointer, hover and click the Info button with A, focus Edit, and move the ScrollBar. Let the loop run 45–60 seconds, exit with PLUS and save `port.log`, `gr-main.log` and a screenshot. Require M17 cycle PASS, positive frame/present counts, `exit_reason=plus`, shutdown PASS and `[BOOT] COMPLETE`. A screenshot can show one representative interaction; log hashes verify the scripted states. A physical run and SD destination hash are not claimed by host or ARM64 evidence.

## Deferred surface

Gameplay callbacks, script execution, audio playback and the full original Windows message loop remain outside M27. Dynamic Image/GraphBuf ownership is analyzed for M28 separately; it is not made interactive by this milestone.

The read-only `m27-dynamic-source-audit.json` classifies all 104 roots with an Image blocker and all 68 physical dynamic GraphBuf controls. Image causes overlap across ancestor roots: 618 empty-key occurrences, 48 GI format/render-semantic occurrences and 2 unresolved cache-key occurrences within those roots. An empty key alone does not prove that game code later sets it. All 68 GraphBuf require runtime data: 19 have a nearby `BindExternalGraphBuf` reference in their form source, 46 have a direct form-code reference to a runtime drawing control, and 3 (`Achievements/ImageMap`, `Number/BGBuf`, `Quest/BGBuf`) have no owner identified by the bounded static search. This is an ownership audit, not a rendering PASS for those controls.

For M28 planning, GAI PBuf is the strongest next candidate: M26 still counts 57 GAI-blocked roots and the portable GAI decode/playback path already exists. Anim/AImage semantics and the remaining visual controls are next. Forms lifecycle, dynamic GraphBuf producers and script/action execution carry broader runtime dependencies; audio follows after action execution. This is a recommendation only; M28 work has not started.
