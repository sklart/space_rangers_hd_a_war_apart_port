# Milestone 11 — GlobalCache and first cached resource

## Scope

M11A creates the upstream `EC_Cache::TCacheEC` implementation after M9 has
loaded DAT roots, user settings and derived runtime state. `GlobalCache` borrows
`CacheDataRoot`; shutdown releases it before the roots. The portable slice uses
only `SetDataRoot`, `OpenDataBuffer` and destruction/`Clear` reached by the
upstream destructor. Specialized cache-data classes, UI, audio and M12 work are
not part of this milestone.

`CacheSize` is parsed as MiB when it is positive and representable (up to 2047
MiB); missing, zero, negative or unsafe values use a documented safe default of
256 MiB. This is a resident-budget setting, not a preallocation.

## M11B evidence

The startup diagnostic walks `CacheDataRoot` in linked entry order, depth-first,
with depth 64 and 100000-entry guards. It selects the first existing file and
reads its bytes exclusively through `GlobalCache->OpenDataBuffer`. The log
records the logical path, backing file, byte size, CRC32, FNV-1a-64 and the
checksum-failure state without dumping asset data.

The asset-free regression builds encrypted synthetic DAT roots, creates
`GlobalCache`, checks its root and configured 64 MiB budget, reads `Base` via
`OpenDataBuffer`, verifies bytes, size and CRC32, then checks cleanup and a
second initialization cycle. CI executes this regression without release assets.

## Dependency boundary

```text
M9 DAT roots/settings/derived state
  -> M11A GlobalCache (TCacheEC, core only)
    -> M11B cached resource proof
      -> M8 renderer self-test
        -> package baseline
```

Hardware validation remains pending. The final artifact provenance is recorded
in `milestone10-build.md` after the final clean ARM64 build and CI run.
