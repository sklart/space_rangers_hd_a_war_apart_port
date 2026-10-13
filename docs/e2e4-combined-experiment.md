# E2E-4 combined Switch experiment

Use `python3 tools/e2e_arm64_build.py --experimental-e2e4-combined` to make one
isolated NRO containing the SDL2 Audio ON bridge, script-width source overlay,
and diagnostics for the next gameplay test. Disk saving is refused for every
save mode until a separate-process round trip validates it. The ordinary E2E
build still has Audio OFF. The combined mode keeps the pinned
`upstream/cpp` checkout unchanged; it compiles disposable source copies.
CI compiles the ordinary build and this one combined experiment. Component
host/sanitizer tests remain separate, but the Switch test handoff will use only
the combined NRO.

The first local combined **debug** build compiled 298 original game units and
linked 379 objects with zero unresolved symbols. Its NRO is a build/link probe,
not a hardware-test candidate. Since that first build, the script overlay has
also gained native-width `TVarEC` values, selected pointer producer/consumer
fixes, and guards against writing wide values into 32-bit script save fields.
The updated overlay completed a second local release-profile ARM64 build:
298 original game units, 379 linked objects, zero unresolved symbols, and
SHA-256 `4D5E3CA325FB9B4B69F17707AE344469AF2204145CC1C94F396750A7F88AEEF0`
for `SpaceRangersHDAWarApartE2E4Combined-ba6c6f8-dirty.nro`. That build used
uncommitted code (`build_dirty=true`). It does not establish new-game, audio
playback, original save/load, or rollback behavior.

Before handing the next combined NRO to a Switch tester, check the original
host path through RuinsTalk and Hangar, the targeted regressions, a clean
release-profile ARM64 link, and matching NRO/ELF hashes. Use that single NRO
for menu, New Game, dialog, Hangar, input, music, and FPS observations. Do not
include save or load in this hardware sequence. Complete the original save/load
round trip and recovery checks before enabling any disk save mode in a later
build. Preserve the user's pre-existing saves.

Current status: **SOFTWARE PARTIAL / HARDWARE PENDING**. No combined hardware
test has been performed.
