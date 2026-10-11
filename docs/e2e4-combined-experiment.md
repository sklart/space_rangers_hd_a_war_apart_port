# E2E-4 combined Switch experiment

Use `python3 tools/e2e_arm64_build.py --experimental-e2e4-combined` to make one
isolated NRO containing the recoverable quicksave path, SDL2 Audio ON bridge,
and the script-width source overlay. The ordinary E2E build still has Audio OFF
and no experimental quicksave rotation. The combined mode keeps the pinned
`upstream/cpp` checkout unchanged; it compiles disposable source copies.
CI compiles the ordinary build and this one combined experiment. Component
host/sanitizer tests remain separate, but the Switch test handoff will use only
the combined NRO.

The first local combined **debug** build compiled 298 original game units and
linked 379 objects with zero unresolved symbols. Its NRO is a build/link probe,
not a hardware-test candidate: the script overlay currently widens the
kind-dependent `TScriptPlace`/`TScriptState::TargetValue` fields and selected
read/write sites, while the wider `TVarEC` and `aScriptFun` pointer paths remain
open. The build was made with uncommitted code (`build_dirty=true`). It does not
establish new-game, audio playback, original save/load, or rollback behavior.

Before handing a combined NRO to a Switch tester:

1. Complete the script producer/consumer and save/load changes described in
   [the pointer-width audit](pointer-width-audit.md), including rejection of
   unresolvable saved pointers.
2. Run host regressions against the original script binding methods, high
   addresses, stale references, original save/load, and quicksave recovery;
   run ASan/UBSan and the full CI, including the combined ARM64 link.
3. Build a clean release-profile combined NRO and record its exact Git SHA,
   SHA-256, and local artifact path.
4. Use that **single** NRO for one hardware sequence covering new game,
   scripted interactions, effects/music, quicksave, reload, interrupted-save
   recovery, and exit diagnostics. Preserve the user's pre-existing saves and
   report each observed check separately.

Current status: **SOFTWARE PARTIAL / HARDWARE PENDING**. No combined hardware
test has been performed.
