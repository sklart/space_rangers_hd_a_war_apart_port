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
| Resource data | `tools/e2e5_resource_validator.py` | CFG checksums, package indexes, selected GI/GAI decoding | Original renderer, all graphic formats, screen first frame |
| Installed GameSettings background | `tools/e2e5_formgameset2_compare.py` plus `tests/test_e2e5_format0_real.cpp` | Independent oracle and production CPU decoder return identical pixels for `FormGameSet2/2bg.gi` | Original `GraphBuf` rescale or GameSettings first frame |
| Installed GameSettings rescale and GI repack | `tools/e2e5_original_graphbuf_host.sh`, `tests/test_e2e5_original_graphbuf.cpp`, `tools/e2e5_gamesettings_rescale.py` | Unchanged original `TGraphBufGR::RescaleWithAspect` runs on installed 1920×1200 pixels; centered 1920×1080 crop gives 1280×720 and FNV64 `ad6d4a9cfa895ba2`, identical to an independent direct OKGF call. Original `TgiGR::CreateFromGraphBuf` generates an RGB565 layer matching the independent oracle on every pixel, FNV64 `8dc3555a82d454db` | Full `EC_CacheGI` call, screen first frame, ARM64 numerical identity |
| Observed game log | `tools/e2e5_scenario_runner.py` | Checkpoints actually present after the New Game transition | Unobserved later stages |
| Crash correlation | `tools/e2e5_crash_analyzer.py` | First explicit log failure, unfinished original log stage, Atmosphere offsets | Causal stack unless matching ELF is supplied |
| Offline PCM | `tools/e2e5_audio_capture.py` | Properties of a supplied signed 16-bit WAV | Full `GR_Music`/`GR_Sound` path or audible quality |
| Installed music oracle | `tools/e2e5_audio_capture.py --game-root ... --resource music/REV/theo_running_away.dat` | Source Ogg independently decodes to PCM; 30-second level/clipping/DC measurements | Original Vorbis worker, mixer, SDL, audible Switch output |
| Host coordination | `tools/e2e5_host_runner.py` | Available tests and explicit missing gates | Complete `ProgramMain` execution |

`tools/e2e5_report.py` combines the levels without elevating resource or
method-level PASS to a game-scenario PASS. The original complete game runtime
does not currently link and run as a host executable. Building that executable
or an equivalent game-runtime harness is a blocker for E2E-5 software acceptance.

## Commands

```text
python tools/e2e5_static_audit.py --check --output reports/e2e5/pointer-width-audit.json
python tools/e2e5_host_runner.py --game-root "D:\\...\\Space Rangers HD A War Apart"
python tools/e2e5_resource_validator.py --game-root "D:\\...\\Space Rangers HD A War Apart" --max-decode 100000 --output reports/e2e5/resource-validation-full.json
python tools/e2e5_crash_analyzer.py D:\\report.zip --output reports/e2e5/crash-analysis.json
python tools/e2e5_report.py --output reports/e2e5/summary.json
```

For the local installed background rescale check on Linux or WSL, run
`bash tools/e2e5_original_graphbuf_host.sh <installed-game-root>`.
It builds original GraphBuf and GI sources with the host OKGF library,
executes the original rescale and repack methods on the installed background,
and repeats under ASan/UBSan with `vptr` excluded.
Public CI runs the same script with `--synthetic`; its report says
`SYNTHETIC`, never `RESOURCE`, when commercial assets are absent.
The host fixture supplies software-only platform stubs and native host math.
The check reads the original asset in place; decoded pixels exist only in a
temporary directory during the run and are not published.

The resource validator's bounded default is a quick check. `--max-decode
100000` requests every indexed GI/GAI item; unsupported formats are reported
as unverified, not passed. A full original renderer comparison and a first
GameSettings frame on the host are still required.

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
