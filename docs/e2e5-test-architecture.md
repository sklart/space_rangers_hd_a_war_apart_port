# E2E-5 test architecture

Status: **SOFTWARE PARTIAL / HARDWARE PENDING**.

The complete [package-host CI run 38155046767](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/runs/38155046767)
passed on code SHA `208d8113fbb3efe0d55cc3aaff829fb409fd9f4f`:
host regression, E2E-5 fast checks, ASan/UBSan, ordinary full ARM64 link,
and the combined experimental ARM64 link. This does not execute the original
complete game scenario or prove console behavior.

The installed commercial game is supplied only through `--game-root` or
`GAME_ROOT`. The tools read it in place and write only metadata, hashes, and
diagnostics to `reports/e2e5/`. No package or decoded image is copied to Git.

| Level | Tool | What it proves | What it does not prove |
|---|---|---|---|
| Static inventory | `tools/e2e5_static_audit.py` | Current C++ source candidates and no newly unclassified site relative to the checked-in baseline | Every candidate is safe; the inventory has unresolved sites |
| Component | `tests/test_e2e5_tools.py` and existing E2E tests | Tool contracts, original method fixtures, targeted native components | Original game scenario |
| Resource data | `tools/e2e5_resource_validator.py` | CFG checksums, package indexes, selected GI/GAI decoding, mapping of the 36 screens registered in original `Globals.cpp` to their UI configs and image dependencies, and source-level resolution of literal `GetByName` references | Original UI construction, dynamic name lifecycle, callbacks, full renderer, all graphic formats, screen first frame |
| Installed GameSettings background | `tools/e2e5_formgameset2_compare.py` plus `tests/test_e2e5_format0_real.cpp` | Independent oracle and production CPU decoder return identical pixels for `FormGameSet2/2bg.gi` | Original `GraphBuf` rescale or GameSettings first frame |
| Installed GameSettings GI decode, rescale, and repack | `tools/e2e5_original_graphbuf_host.sh`, `tests/test_e2e5_original_graphbuf.cpp`, `tools/e2e5_gamesettings_rescale.py` | Original `TBufEC` accepts and preserves the source GI; `TgiGR::LoadRawGiFromBuffer` and `DecodeToGraphBuf` produce the independent Format-0 oracle's pixels. The generated Switch `TGraphBufGR::RescaleRGBA_HW` override delegates to unchanged original `RescaleWithAspect`, center-crops 1920×1200 to 1920×1080 and gives 1280×720, x86 NATIVE FNV64 `ad6d4a9cfa895ba2`, identical to a direct same-architecture OKGF call. Original `TgiGR::CreateFromGraphBuf` generates RGB565 matching the independent oracle on every pixel, FNV64 `8dc3555a82d454db`; a new `TBufEC` retains those bytes after the temporary GI object is freed and all fixture objects run their original destruction path | Cache load, screen first frame, Switch runtime |
| Original GameSettings cache load | Same host script and ARM64/QEMU script, built with `E2E5_ORIGINAL_CACHEGI` | Unchanged original `TCGiEC::ApplyWideScreenLayoutFixups` and `LoadFromConfigBuffer` run on the installed `2bg.gi` with the generated Switch GraphBuf override. A real cache object loads the transformed 1280×720 RGB565 GI into `Image`, records `ResidentBytes`, and is freed. Its 1,843,200-byte payload matches the independent x86 oracle (`8dc3555a82d454db`) on x86 and the separate original ARM64 GraphBuf/GI result (`1107dc234c3883f5`) under QEMU. Expected rescale start and completion log text is observed for both calls. Synthetic and installed x86 runs passed normally and with ASan/UBSan (`vptr` excluded); synthetic and installed ARM64 runs passed under QEMU. | UI object creation, first screen frame, Switch runtime |
| Observed game log | `tools/e2e5_scenario_runner.py` | Checkpoints actually present after the New Game transition | Unobserved later stages |
| Crash correlation | `tools/e2e5_crash_analyzer.py` | First explicit log failure, unfinished original log stage, Atmosphere offsets | Causal stack unless matching ELF is supplied |
| Offline PCM | `tools/e2e5_audio_capture.py` | Properties of a supplied signed 16-bit WAV | Full `GR_Music`/`GR_Sound` path or audible quality |
| Installed music oracle | `tools/e2e5_audio_capture.py --game-root ... --resource music/REV/theo_running_away.dat` | Source Ogg independently decodes to PCM; 30-second level/clipping/DC measurements | Original Vorbis worker, mixer, SDL, audible Switch output |
| Host coordination | `tools/e2e5_host_runner.py` | Available tests and explicit missing gates | Complete `ProgramMain` execution |
| Original process smoke on Linux x86-64 | `tools/e2e5_host_game_build.py`, `tools/e2e5_host_game_main.cpp`, and `tools/e2e5_host_game_probe.py` | Reproducible full-effective host ELF ran original `ProgramMain` with installed assets, submitted a 1280×720 RGB565 loading frame, opened Main Menu, processed original Return-key actions in the menu and GameSettings, opened Introduction and ran its presentation loop while generation was active. After native-width repairs, the latest run survived a 180-second probe window without a signal. Original music workers played decoded data into SDL dummy audio. The builder records its 331-object source map and binary SHA-256; the probe records isolated logs. | No confirmed generation completion, gameplay, independently captured GameSettings pixel frame, PCM quality or Switch runtime proof. |

`tools/e2e5_report.py` combines the levels without elevating resource or
method-level PASS to a game-scenario PASS. An exploratory full-effective
Linux x86-64 ELF links 296 original units with the combined disposable source
overrides and host platform units. With installed resources, it first dispatched
screen 22 (loading), submitted a 1280×720 RGB565 frame with observed hash
`a09bebcd390a8da6`, then dispatched screen 1 (Main Menu) and completed its
`OnOpen`. ELF SHA-256 was
`96f7eed8a6ac5b2dccd60fc798cfdd8caf8cc74423c2504a8b4dd9409fb5de2e`.
The hash can vary with animation and is not a visual oracle. The exploratory
ELF was assembled manually. The reproducible full-effective host build later
linked 296 original units, 34 platform units, and a test-only entrypoint (ELF
SHA-256 `06760dcf95bdd97f2e87e950512b00a6cda301b4aaf10cd80650fb046ee3f0c1`).
Its isolated probe observed original input dispatch from menu screen 1 to New
Game screen 33, successful GameSettings `OnOpen`, all six background
decode/scale/repack checkpoints, and first, second, tenth and sixtieth frame
attempts. A second original Return-key action dispatched Introduction screen 3,
which also opened and presented frames while the generation worker ran.
Native-width repairs then moved the observed crash through several later
generation paths. Clearing the original player's equipment slots at their
native offsets let the latest host run reach its 180-second timeout without
a signal. Generation completion and gameplay remain open.

## Commands

```text
python tools/e2e5_static_audit.py --check --output reports/e2e5/pointer-width-audit.json
python tools/e2e5_host_runner.py --game-root "D:\\...\\Space Rangers HD A War Apart"
python tools/e2e5_resource_validator.py --game-root "D:\\...\\Space Rangers HD A War Apart" --max-decode 100000 --output reports/e2e5/resource-validation-full.json
python tools/e2e5_crash_analyzer.py D:\\report.zip --output reports/e2e5/crash-analysis.json
python tools/e2e5_report.py --output reports/e2e5/summary.json
```

On Linux/WSL, `python3 tools/e2e5_host_game_build.py` builds a host ELF in the
ignored `port/switch/build/e2e5-host-game/reproducible` directory. It uses the
same disposable source generators as the combined experimental ARM64 build,
compiles the complete original game except the host-replaced entrypoint and
file adapter, then records source paths and hashes in `report.json`. This build
is a Linux diagnostic; its success does not establish Switch behavior.

`python3 tools/e2e5_host_game_probe.py --binary <host-elf> --game-root
<installed-game> --library-dir <host-okgf-dir> --timeout 100` runs an existing
ELF in a fresh ignored build directory. It stops after the first accepted
software frame and preserves `report.json` plus the logs in that directory.
`--auto-new-game` sends one Return key through the host message queue only
after the original Main Menu `OnOpen` marker; it waits for New Game screen 33
and its first frame attempt or stops at the timeout. The exact outcome is
reported without promoting an attempted frame to visual or hardware proof.
`--auto-start-game` sends a second Return key after 60 GameSettings frames
through the same queue and waits for generation completion and an Introduction
frame or a timeout. All writes remain inside the isolated test user root.
The entrypoint rejects a user root symlink and keeps the installed game root
separate from the test user root. `E2E5_HOST_SDL_WINDOW` enables an SDL dummy
window and first-frame marker on host only; together with
`E2E_HOST_RESOLVER_TEST` it avoids the original startup's recursive attribute
change beside its executable.

For the local installed background rescale check on Linux or WSL, run
`bash tools/e2e5_original_graphbuf_host.sh <installed-game-root>`.
It builds the generated Switch GraphBuf override and original CacheGI, GI,
buffer, string, and runtime sources with the host OKGF library,
executes the original layout fixup, decode, rescale, and repack methods on the installed background,
and repeats under ASan/UBSan with `vptr` excluded.
Public CI runs the same script with `--synthetic`; its report says
`SYNTHETIC`, never `RESOURCE`, when commercial assets are absent.
The host fixture supplies software-only platform stubs and native host math.
GI decoders for formats other than Format 0 abort if reached in this fixture.
The check reads the original asset in place; decoded pixels exist only in a
temporary directory during the run and are not published.

For a repeatable Linux ARM64 component run, install QEMU user mode, the
`aarch64-linux-gnu` GCC/G++ cross compiler, CMake, JPEG and PNG development
headers, then run `bash tools/e2e5_original_graphbuf_arm64_host.sh
<installed-game-root>`. The `e2e5-arm64-host` CI job runs the same script
with `--synthetic` when private resources are unavailable.

A local Linux ARM64 build of these original GI, generated GraphBuf override,
buffer, structure, and runtime objects also ran under QEMU on the installed
background. The
minimal ARM64 OKGF library used the Switch `COMPATIBLE` portable math mode.
The generated Switch `RescaleRGBA_HW` override was invoked; its original
GraphBuf output matched a direct ARM64 OKGF rescale byte for byte,
and original GI repacking matched direct ARM64 RGB565 conversion byte for byte.
The original `TCGiEC::ApplyWideScreenLayoutFixups` and
`LoadFromConfigBuffer` also produced the same ARM64 RGB565 bytes, loaded the
cache image, and completed their expected log markers.
Its decoded source FNV64 was `decfdd81e05299cd`, scaled BGRA FNV64
`72611b035290c515`, and RGB565 FNV64 `1107dc234c3883f5`. The scaled and
RGB565 hashes differ from x86 NATIVE results, so the wrapper reports
`cross_architecture_hash_equal: false` while retaining the same-architecture
method comparison. QEMU executes Linux ARM64 code, not Horizon/libnx or the
full game. This does not prove the Switch screen or pixel identity with x86.

The resource validator's bounded default is a quick check. `--max-decode
100000` requests every indexed GI/GAI item; unsupported formats are reported
as unverified, not passed. A bounded run remains `PARTIAL` when it leaves
images unexamined, even if every sampled decode succeeds. Missing image
dependencies also keep the result `PARTIAL`. A full original renderer comparison and a first
GameSettings frame on the host are still required.
For a GAI sequence source outside the frame table, the report includes the
sequence, position, source index, frame count and delay. This remains a
resource-level error until original playback establishes whether the entry is
an intentional empty frame.

The named-control audit uses the original registered screen's source module
and the installed `Main.dat`. It counts literal `GetByName` names, resolves
explicit `Globals::<other screen>` receivers against that screen's config,
and marks a missing config name as source-created only when the module has a
matching literal `SetName`. It does not execute construction or prove the
created object exists at each call site.

The ARM64 builder now uses GCC dependency files per object. The generated
source copy is rewritten only on content changes; a change in one disposable
override should not rebuild unrelated translation units. A clean CI build
continues to compile all original objects. Upstream source remains unchanged.

## Evidence rules

`PASS` applies only to the row's named level. `PARTIAL` means some operations
were checked and at least one required format or path remains unverified.
`NOT RUN` and `NOT REACHED` are never promoted to success. The final NRO is
withheld until the software gates are complete; hardware remains pending until
one cumulative build is exercised on the console.
