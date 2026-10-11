# E2E-5 coverage matrix

| Requirement | Current evidence | State |
|---|---|---|
| Pointer width across original sources | 1,389 modules scanned; 2,523 candidates, 30 confirmed UI pointer writes, incremental CI guard | PARTIAL: 2,401 unresolved candidates need semantic review |
| Installed CFG/package inventory | `Main.dat`, `CacheData.dat`, 20 packages, 9,129 entries, 41 screen groups; 1,148 of 1,152 screen image options resolve to package paths | PARTIAL: four options need dynamic-resource classification |
| GI/GAI pixel and renderer comparison | Full batch of 4,695 suffix matches; 4,694 rows examined, including 479 Format-0 and 3,054 Format-2 GI files; 224 formats or GAI frames remain unsupported by the independent oracle. The installed `FormGameSet2/2bg.gi` has identical pixels under the independent Python oracle and production C++ Format-0 decoder: 1920×1200, FNV64 `decfdd81e05299cd`. The unchanged original `TGraphBufGR::RescaleWithAspect` and independent host OKGF call both rescale the centered 1920×1080 crop to 1280×720, FNV64 `ad6d4a9cfa895ba2`; the method also passed ASan/UBSan with `vptr` excluded. RGB565 conversion matched all pixels, FNV64 `8dc3555a82d454db`. | FAIL for one GAI sequence; original GI repack, rendering, first frame, and ARM64 result remain unverified |
| GameSettings first frame | Previous hardware log stopped at `Rescaling Bm.FormGameSet2.2bg...`; experimental source override now logs entry and exit of the original GI decode, rescale, and repack calls; ARM64 build passed | NOT RUN on host; new trace not yet measured on hardware |
| Main Menu → New Game → gameplay | Checkpoint log evaluator | NOT RUN on host |
| Full original audio path and offline capture | Existing component tests; ring underrun/reject counters; independent 30-second decode of installed menu track. Experimental source override distinguishes poll timeout `3` from real completion event `0` and retains one late decoded chunk for retry; ARM64 link passed. | NOT RUN end to end; audio output and audible quality unverified |
| Cursor and frame stage profiler | Experimental ARM64 `GI_MessageLoop::Run` stage timer and separate five-second input poll/move/gap counter compiled; earlier hardware presentation rate only | PARTIAL: no measured stage or input cadence log yet |
| Original script and save/load round-trip | Existing native-width component tests and guarded save | NOT RUN end to end |
| Crash correlation | `D:\\report.zip` parsed; exact first exception was not captured in log | PARTIAL |
| Incremental ARM64 build | Per-object dependencies added; a local rebuild compiled 1 object and reused 379; clean CI linked the ordinary game and the combined experiment | PASS for build mechanics |
| ASan/UBSan and full CI | All four jobs passed at code SHA `208d8113fbb3efe0d55cc3aaff829fb409fd9f4f` in [package-host run 38155046767](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/runs/38155046767). The new original GraphBuf host fixture passed locally under ASan/UBSan with `vptr` excluded on installed and synthetic pixels; the synthetic CI gate is added but its first run is pending. | PASS for previously named CI jobs; current HEAD full CI and original game scenarios still pending |
| One final console NRO | None for E2E-5 | BLOCKED by software gates |

The source inventory is a search aid, not proof that all 2,523 candidate
expressions narrow pointers. The current automated semantic classification is
insufficient to close the pointer-width audit.
