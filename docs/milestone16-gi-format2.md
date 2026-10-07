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
