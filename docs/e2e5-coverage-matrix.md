# E2E-5 coverage matrix

| Requirement | Current evidence | State |
|---|---|---|
| Pointer width across original sources | 1,389 modules scanned; 2,523 candidates, 30 confirmed UI pointer writes, incremental CI guard | PARTIAL: 2,401 unresolved candidates need semantic review |
| Installed CFG/package inventory | `Main.dat`, `CacheData.dat`, 20 packages, 9,129 entries, 41 screen groups; 1,148 of 1,152 screen image options resolve to package paths | PARTIAL: four options need dynamic-resource classification |
| GI/GAI pixel and renderer comparison | Full batch of 4,695 suffix matches; 4,694 rows examined, including 479 Format-0 and 3,054 Format-2 GI files; 224 formats or GAI frames remain unsupported by the independent oracle. The installed `FormGameSet2/2bg.gi` has identical pixels under the independent Python oracle and production C++ Format-0 decoder: 1920×1200, FNV64 `decfdd81e05299cd`. | FAIL for one GAI sequence; original `GraphBuf` render and rescale remain unverified |
| GameSettings first frame | Previous hardware log stopped at `Rescaling Bm.FormGameSet2.2bg...`; experimental source override now logs entry and exit of the original GI decode, rescale, and repack calls; ARM64 build passed | NOT RUN on host; new trace not yet measured on hardware |
| Main Menu → New Game → gameplay | Checkpoint log evaluator | NOT RUN on host |
| Full original audio path and offline capture | Existing component tests; ring underrun/reject counters; independent 30-second decode of installed menu track. Experimental source override distinguishes poll timeout `3` from real completion event `0` and retains one late decoded chunk for retry; ARM64 link passed. | NOT RUN end to end; audio output and audible quality unverified |
| Cursor and frame stage profiler | Experimental ARM64 `GI_MessageLoop::Run` stage timer compiled; earlier hardware presentation rate only | PARTIAL: no measured stage log or separate cursor frequency yet |
| Original script and save/load round-trip | Existing native-width component tests and guarded save | NOT RUN end to end |
| Crash correlation | `D:\\report.zip` parsed; exact first exception was not captured in log | PARTIAL |
| Incremental ARM64 build | Per-object dependencies added; latest trace build compiled 2 objects and reused 378; complete rebuild verification pending | PARTIAL |
| ASan/UBSan and full CI | Existing E2E-4 jobs plus new E2E-5 fast job | PENDING current branch run |
| One final console NRO | None for E2E-5 | BLOCKED by software gates |

The source inventory is a search aid, not proof that all 2,523 candidate
expressions narrow pointers. The current automated semantic classification is
insufficient to close the pointer-width audit.
