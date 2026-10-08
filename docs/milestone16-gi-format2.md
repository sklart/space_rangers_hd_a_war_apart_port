# M16 — portable GI Format-2 CPU decode

M16 adds a bounded, CPU-only decoder for GI Format 2. It validates the 64-byte
GI header, destination bounds, plane table, each present plane and every RLE
command before calling the pinned OKGF CPU routines. Draw order matches
upstream: plane 2 Alpha, plane 1 TransAlpha, then plane 0 Trans.

`DATA/Asteroid/00.gai` is the fixed release resource: 246,863 bytes, 100 raw
Format-2 frames, Flags 0 and one sequence. The independent Python oracle fixes
frame 0 at 33x40, pitch 132, pixels CRC32 `83f66519`, FNV-1a-64
`eb000366ca288b23`; the canonical 100-frame stream is CRC32 `9e4059ce`,
FNV-1a-64 `a028ffbf04472afa`.

The Switch diagnostic processes one `CpuImage` at a time. M16 does not
implement animation playback, sequence timing, Flags!=0 composition, formats
1 or 3–6, textures/surfaces, Forms or UI.

The M16 ARM64 objects have no `GR_DX`, Direct3D, `TGraphBufGR`, `TgiGR`,
`TCGaiEC` or `GI_GAIFile` symbols. The complete inherited M12 runtime ELF
still contains `GR_DX`/`TGraphBufGR` for its pre-existing RGB565 renderer, so
a full-binary absence audit is not an M16 PASS criterion without separately
changing that already-hardware-tested renderer boundary.

GitHub Actions run `37623625785` passed the synthetic decoder, corrupt stream
suite and the separate M16 host symbol audit. M16 is complete: the Switch
hardware result below matches the independent oracle.

## Switch hardware result

**Runtime PASS** — the Switch log recorded `build_git=50b778c`, then decoded
all 100 frames with frame 0 CRC32/FNV `83f66519`/`eb000366ca288b23` and
aggregate CRC32/FNV `9e4059ce`/`a028ffbf04472afa`. The M12 loop subsequently
ran 943 frames/presents for 47,272 ms, exited through `PLUS` and reached
`[BOOT] COMPLETE`. The tested `Space Rangers HD - A War Apart.nro` has SHA-256
`470246273738C66777672E0D88A3449DA3FF47398AC08C16BB3175B5CAE8E08E`.
