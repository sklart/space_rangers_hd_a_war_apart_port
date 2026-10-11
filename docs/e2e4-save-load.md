# E2E-4 save/load audit and safe replacement test

The original `aSaveLoad::TSaver_Execute` writes a complete `save.tmp` through
`TFileEC`, then rotates quicksave slots, deletes an existing target `.sav`,
and calls `MoveFileW(save.tmp, target)` without checking its result. A failed
final rename can leave the previous target missing. Quicksave rotation also
deletes the destination slot before checking each rename. `TFileEC::WriteBuffer`
raises on a failed/short write, while `ReleaseHandle` closes the handle; the
portable adapter does not establish power-loss durability on an SD card.

`e2e4_save_replace.hpp` contains a **test-only candidate** that validates a
nonempty, closed temporary file, renames the old target to a previously unused
backup name, installs the new file, and attempts to restore the old one on a
failed install. It never deletes a prior save or backup. The host test covers
first and repeated replacement, backup retention, failed install with
rollback, missing and empty temp files. The helper is not wired into
`TSaver_Execute`; the active game path remains as described above. A proposed
integration touching quicksave deletion/rotation was rejected by automatic
approval review because it could irreversibly lose user saves, so no runtime
save behavior was changed.

The existing `.sav` format and 32-bit IDs are untouched. No whole-game
save→load round-trip, version rejection, Extended numeric comparison or
interrupted SD-card write has been verified. Status: **PARTIAL**.
