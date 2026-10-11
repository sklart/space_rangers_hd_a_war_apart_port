# E2E-4 audio test evidence

The asset-free `host-e2e4-wav-test` covers mono unsigned 8-bit, mono signed
16-bit and stereo signed 16-bit PCM at 11.025, 22.05, 44.1 and 48 kHz. It
checks the returned PCM span and sample bytes, including a misleading `data`
sequence inside an odd-length `JUNK` chunk. Invalid RIFF, truncated chunks,
absent `fmt`/`data`, zero rate, non-PCM format, wrong alignment, partial
frames, oversized chunk metadata and embedded-buffer offsets are rejected.
A deterministic malformed-input sweep checks span bounds. This parser is used
by the disposable `EC_CacheSound.cpp` source override; the upstream submodule
remains unchanged.

`host-e2e4-vorbis-read-test` drives the bounded read helper with a synthetic
decoder. A 352,800-byte music request is split into calls of at most 4096
bytes, with exact output and guard bytes checked. It distinguishes EOF from a
negative decoder error and rejects oversized or partial-frame decoder returns.
The disposable original `VorbisFile.cpp` now uses that helper and returns
complete-element counts in `ReadVorbisSource`. The ARM64 object compiles.

Local 2026-10-11: both host executables passed with MSYS2 GCC; the generated
`EC_CacheSound.cpp` and `VorbisFile.cpp` compiled with devkitA64 GCC 16.1.0.
The installed MSYS2 GCC lacks `libasan` and `libubsan`, so sanitizer execution
was not available in that environment. The original `TOggWorker::VorbisState`
is still a 720-byte opaque Win32 buffer; therefore actual ARM64 Vorbis decode,
streaming, music transitions and audible output are **NOT VERIFIED**.

The CI host regression job includes both new tests. Audio ON must remain
unclaimed until the native Vorbis state, SDL2 mixer, DirectSound-compatible
buffer semantics and event/timer path are implemented and exercised together.
