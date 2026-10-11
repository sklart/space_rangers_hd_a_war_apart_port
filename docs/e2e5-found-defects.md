# E2E-5 defect register

| ID | Evidence | Current state |
|---|---|---|
| E2E5-001 | Previous Switch log ended at `Rescaling Bm.FormGameSet2.2bg...` after GameSettings `OnOpen ready`; primary exception text absent | Open; E2E-4 CPU rescale override compiled but has no hardware confirmation |
| E2E5-002 | Three earlier `[MUSIC] stream write rejected chunk=2 bytes=352800` lines and audible distortion | Open; larger stream chunk and late refill are unverified on hardware |
| E2E5-003 | Earlier menu presentation rate 4.32–5.18 FPS; average present about 9.7–10.4 ms | Open; frame stage breakdown absent |
| E2E5-004 | Static inventory includes unresolved script pointer and callback conversions | Open; native-width representation is only partially implemented |
| E2E5-005 | First game exception was not written to the prior `e2e.log`; Atmosphere Instruction Abort was later cleanup evidence | Open; primary exception capture needs validation |
| E2E5-006 | 30 original UI writes cast live pointers to `int32_t` in multiple screens (`fCfgSettings`, shops, panel, rewards, conversations and others) | Native-width disposable override implemented; ARM64 and original behavior validation pending |
| E2E5-007 | `WSE.pkg:DATA/Weapon/Effect/05.GAI` has a sequence table rejected by the independent oracle; `CacheData` references it as `Bm/Hits/05` | Open; compare original parser and combat reachability |
| E2E5-008 | `common.pkg:DATA/Galaxy/war.gai` is a 14-byte non-image payload despite its suffix | Classified as non-image metadata; no renderer test implied |

The crash archive is retained outside the repository. See
`reports/e2e5/crash-analysis.json` for file offsets and provenance.
