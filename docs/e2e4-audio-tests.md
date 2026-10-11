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

`tests/fixtures/e2e4_sine_22050_stereo.ogg` is a self-generated 0.3-second
440-Hz tone (FFmpeg 9.0.2: `-f lavfi -i
sine=frequency=440:duration=0.3:sample_rate=22050 -ac 2 -c:a libvorbis
-q:a 1 -map_metadata -1`). Its SHA-256 is
`C7C2ACFEFAD578E46FF5D3FF4EBF42BD5688E071076B659DA42152F650060CDD`.
The native decoder test opens it through a complete-element memory callback,
checks 22.05-kHz stereo output, nonzero decoded PCM, EOF, guard bytes,
reopen, damaged input and source-read failure. The Switch decoder,
`VorbisFile.cpp` override and `GR_Music.cpp` cleanup override compile as
ARM64 objects. Linux CI runs the fixture test under ASan/UBSan and in the
host regression job.

Local 2026-10-11: both host executables passed with MSYS2 GCC; the generated
`EC_CacheSound.cpp` and `VorbisFile.cpp` compiled with devkitA64 GCC 16.1.0.
The installed MSYS2 GCC lacks `libasan` and `libubsan`, so sanitizer execution
was not available in that environment. The original `TOggWorker::VorbisState`
remains a 720-byte opaque field but holds only a native decoder pointer on
Switch. Full game streaming, music transitions and audible output are
**NOT VERIFIED**.

The CI host regression job includes both new tests. Audio ON must remain
unclaimed until the native decoder, mixer integration, DirectSound-compatible
buffer semantics and event/timer path are exercised together.

`host-e2e4-audio-mixer-test` verifies numerical output for 8-bit mono and
16-bit stereo, 22.05→44.1 kHz resampling, 48→44.1 kHz playback, simultaneous
voices, clipping, separated music/effects mute, pan, two-frame fade, looping,
stop, stale-handle rejection and deferred PCM reclamation. It passed locally
with MSYS2 GCC. The mixer test and SDL2 device wrapper both compile with
devkitA64; host SDL2 output and physical audibility have not been tested.

After the combined `c87e239` Switch run reported distorted menu music, a
separate host regression was added for two loud effects opposed by a music
sample. It checks that float accumulation yields the correct 0.75 result
before the final S16 clamp; the former two-stage clamp yielded about 0.25.
The mixer and three-chunk stream tests pass with MSYS2 GCC and with ASan/UBSan
on WSL. `e2e4_sdl_audio.cpp` passes an ARM64 syntax check. This fixes a
confirmed mixing error but does not explain the two logged stream-refill
failures or establish audible quality on Switch.
