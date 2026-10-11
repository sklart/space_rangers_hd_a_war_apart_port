# E2E-4 quicksave transaction (experimental)

Status: **SOFTWARE PARTIAL / HARDWARE PENDING**. The ordinary Switch build
refuses quicksave before it writes `save.tmp`; the recoverable rotation is
compiled only with `python3 tools/e2e_arm64_build.py --experimental-quicksave`.
The original upstream `aSaveLoad.cpp` is unchanged. The experimental branch is
inserted into a disposable generated source file. Other save modes retain
their original behavior and are outside this quicksave gate.

## Format check before changing slots

The original worker closes `save.tmp` before the transaction starts. The
transaction checks that it is a regular file in the trusted save directory,
is readable and bounded, has all eight terminated UTF-16 header fields with
`RSG` and `EZ` signatures, and contains bounded preview and game-state
blocks. It decrypts the compressed game-state block using the original seed
algorithm, validates its zlib stream, and compares the original CRC32. The
optional film-history tail has no independent checksum; a full original
save/load round trip is still required on hardware. If validation fails,
every old slot remains unchanged. An existing `save.tmp` is never replaced
at the beginning of a new experimental quicksave.

## Transaction and recovery

`port/switch/platform/e2e4_quicksave_transaction.cpp` snapshots every present
slot from `QuickSave.sav` through the configured final extra slot into a new
`.e2e4-quicksave-backups/txn-NN/slotN.orig` directory. Each copy is flushed
and checked against the size and CRC32 of the source. A flushed `manifest`
records which slots existed and their fingerprints. Only after every snapshot
and the manifest is complete does the transaction remove old slot names and
publish new copies without overwriting any occupied destination. It never
renames a file over another file. A `committed` marker completes the rotation;
the old snapshots remain available. Existing `.sav`, `.bak`, and transaction
names are never reused or overwritten. Symlinks, special files, unexpected
paths, and unsafe parent directories are rejected.

On a failed operation after the manifest, rollback quarantines any newly
published slot as `slotN.interrupted` (then numbered names if a retry finds
another partial file) and restores original slots from the
snapshots. Startup of the experimental build and the next quicksave also
scan pending transactions and retry recovery. If recovery cannot complete,
it stops and reports the transaction path; snapshots and `save.tmp` remain.
`save.tmp` is deleted only after a successful quicksave commit. A failed or
interrupted save leaves it in place and the next save refuses to overwrite it.
Each attempt consumes one of 16 transaction directories; at the limit, saving
fails before changing old slots. No automatic cleanup policy is applied.

## Host evidence and remaining gates

The host regression test covers successful three-slot rotation, prior `.bak`
preservation, validation failure, the 16-directory limit, symlink refusal,
injected read failures and failure at every mutation, simulated process
interruption at every stage, a persistent simulated out-of-space failure
during publish and rollback, and an interrupted or partial rollback retried
from disk.
ASan/UBSan runs the
same test in CI. An isolated ARM64 experimental NRO is built by CI. These
checks do not establish SD-card power-loss durability, a real storage-full
event, original game save/load compatibility, or preservation of slots on
physical Switch hardware. Directory-entry durability on the Switch SD
filesystem has not been established. Keep the experimental NRO off user data
until these gates are observed.

## Manual recovery

Stop the game and make an external copy of the entire `save` directory before
touching any file. Inspect the newest `save/.e2e4-quicksave-backups/txn-NN`:

1. If `committed` exists, the rotation completed. `slotN.orig` are the prior
   saves; do not change the active slots unless intentionally reverting.
2. If `rolled-back` exists, original slots were restored. Keep the directory
   until its files and the active slots have been checked.
3. If neither marker exists and `manifest` exists, read its presence column.
   Preserve every current `QuickSave*.sav` under a new, unused name outside
   the active slot names. For each row marked `1`, copy `slotN.orig` back to
   the corresponding `QuickSave*.sav` only when that destination is absent.
   Rows marked `0` should have no active slot after current files are moved.
   Never replace a snapshot or an existing `.bak`.
4. If `manifest` is absent or malformed, preserve all files and inspect the
   active slots. The implementation writes the manifest before removing any
   slot, but a later filesystem error could also damage it; do not infer that
   a partial `slotN.orig` is complete.

If automatic rollback failed, the log names the retained transaction
directory and the failed operation. Keep `save.tmp`, all `slotN.orig`, and
all `slotN.interrupted*` files for comparison. Do not remove backup directories
merely to clear the 16-directory limit; cleanup policy needs separate approval.
