# E2E-5 coverage matrix

| Requirement | Current evidence | State |
|---|---|---|
| Pointer width across original sources | 1,389 modules scanned; 2,523 candidates, 30 confirmed UI pointer writes, incremental CI guard | PARTIAL: 2,401 unresolved candidates need semantic review |
| Installed CFG/package inventory | `Main.dat`, `CacheData.dat`, 20 packages, 9,129 entries, 41 screen groups; 1,148 of 1,152 screen image options resolve to package paths | PARTIAL: four options need dynamic-resource classification |
| GI/GAI pixel and renderer comparison | Full batch of 4,695 suffix matches; 4,693 parsed; 737 unsupported by oracle; `FormLoad2/2BarCenter.gi` Format‑2 decoded | FAIL for one GAI sequence; other formats and renderer oracle pending |
| GameSettings first frame | Original method fixture and installed metadata; prior hardware stopped at rescaling `Bm.FormGameSet2.2bg` | NOT RUN on host |
| Main Menu → New Game → gameplay | Checkpoint log evaluator | NOT RUN on host |
| Full original audio path and offline capture | Existing component tests; ring underrun/reject counters; independent 30-second decode of installed menu track | NOT RUN end to end |
| Cursor and frame stage profiler | Experimental ARM64 `GI_MessageLoop::Run` stage timer compiled; earlier hardware presentation rate only | PARTIAL: no measured stage log or separate cursor frequency yet |
| Original script and save/load round-trip | Existing native-width component tests and guarded save | NOT RUN end to end |
| Crash correlation | `D:\\report.zip` parsed; exact first exception was not captured in log | PARTIAL |
| Incremental ARM64 build | Per-object dependencies added; complete rebuild verification pending | PARTIAL |
| ASan/UBSan and full CI | Existing E2E-4 jobs plus new E2E-5 fast job | PENDING current branch run |
| One final console NRO | None for E2E-5 | BLOCKED by software gates |

The source inventory is a search aid, not proof that all 2,523 candidate
expressions narrow pointers. The current automated semantic classification is
insufficient to close the pointer-width audit.
