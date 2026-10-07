# M15 — portable GAI container and one real Format-0 frame

M15 adds a CPU-only reader for the on-disk GAI container. `gai_cpu` never casts a byte buffer to an upstream packed type: all fields are decoded by bounded little-endian reads, and local disk-layout assertions document the six fixed record sizes (48, 8, 8, 8, 4 and 8 bytes).

The parser validates the header, frame directory, each non-empty frame range, and the structural sequence-table range. Empty frames are represented but cannot be decoded. Frame payloads may be raw GI, `ZL01`, or `ZL02`; decompression is capped at 256 MiB and is not success by itself — the result must pass the existing M14P Format-0 validator and decoder.

## Release oracle and baseline

`tests/probe_gai_release.py` is an independent Python oracle. It reads `DATA/common.pkg`, validates GAI directly, inflates wrappers with Python `zlib`, and independently decodes Format-0 RGB565 into BGRA. It does not import any M15 C++ source.

The preferred discovery resource is `DATA/Asteroid/00.gai`:

- 246,863 bytes; CRC32 `045269e4`; FNV-1a-64 `f6473afbb3d4b1f4`.
- version 1, bounds 0,0,48,48, Flags 0, 100 raw frames, one sequence.
- all 100 embedded GI frames are Format 2, so this resource is inventory-only for M15.

The deterministic lexical fallback selected `DATA/BGObj/bg00.gai`, frame 0:

- GAI: 8,000,152 bytes; CRC32 `9e05776f`; FNV-1a-64 `03f332f6307d4031`; version 1; bounds 0,0,2000,2000; Flags 0; one frame; no sequence table.
- frame: offset 56, stored size 8,000,096, raw GI.
- GI: 8,000,096 bytes; CRC32 `05d665d2`; FNV-1a-64 `3ccdac34b2d0a2cc`; Format 0; RGB565 masks `0000f800,000007e0,0000001f,00000000`.
- CPU output: 2000x2000 BGRA, pitch 8000; CRC32 `3fc81562`; FNV-1a-64 `ad9d67c6c7ad85b9`.

The Switch diagnostic uses only this fixed resource/frame after M14P and before the M12 loop. It performs no full resource scan on the console.

## Deliberate exclusions

M15 does **not** implement animation playback, sequence timing, Flags!=0 cumulative composition, GAI surfaces/textures, cache workers, `TCGaiEC`, `GI_GAIFile`, or UI animation objects. Formats 1–6 remain inventory-only. The direct M14 and M14R `GR_DX`/`TGraphBufGR` blockers are unchanged.

## Evidence state

The release baseline, M15-specific GitHub Actions host regression, clean ARM64 build, and Switch hardware test have passed. The first Switch attempt exposed a truncated FNV-1a offset basis; the corrected NRO `9d8d8bb` matched all oracle CRC32/FNV values for the real fixed frame, ran the M12 loop for 2,003 frames in 100,264 ms, exited through `PLUS`, and reached `[BOOT] COMPLETE`.
