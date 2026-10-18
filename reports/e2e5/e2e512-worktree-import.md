# E2E-5.12: audit of uncommitted worktrees

The six existing worktrees were inspected without changing them. Their HEAD,
binary Git diff, changed-file SHA-256 inventory, and file archives are saved
locally under `build/e2e512-worktree-import/` (ignored by Git). These snapshots
are recovery evidence, not release artifacts.

| Worktree | HEAD | Result on `master` |
| --- | --- | --- |
| `e2e4-offline-runtime-audio` | `587f7462083cfb7a0efb37b4fb8e3249ff6c2128` | Options mode initialization, GAI bounds, probe, and renderer changes were already integrated; later diagnostics supersede its older profiling snippets. The corrected Options/RuinsTalk analysis is already tracked. |
| `e2e5-p0-startup` | `65c0cc53b83f` | Clean; no uncommitted changes. |
| `e2e511-flight-input` | `0f91c1a` | Arcade commands, input mapping, frame capture, and source overrides were already integrated and subsequently refined. |
| `m23-font-label` | `30a255734165` | 166 status entries, zero-byte `git diff --binary` and no whitespace-insensitive diff; no source change to transfer. |
| `m26-ui-next` | `3715e522e6b6` | Only line-ending dirt in submodule `upstream/cpp` (`EC_BlockPar.cpp`, `EC_Data.cpp`); nested binary diff is empty. |
| `stabilization-20261018` | `0c900810af7260bff7ef62610ff6129373ee8897` | Imported the missing UI progress instrumentation, Script VM contract, repeated map/menu probe, and host frame capture; integrated with newer host driver and CI. |

The existing `saves/` directory and the separate untracked
`reports/e2e5/e2e510-artifact-provenance.json` were not modified or staged.
The actual merge candidate still requires the final host, ARM64, and artifact
checks on its new commit; this inventory alone is not hardware proof.
