# E2E-4 audio architecture and baseline audit

Baseline: `4210fd5d3852d50bc336ec0a68315ecd35f19a2c`, upstream C++
`57fa689c630193a66fdea6ca4c79a188814991cd`. The original NRO files and
their recorded hashes are outside this worktree and are not build inputs.

## Original path

`GR_Main::InitializeRuntimeAndSettings` loads `SoundVolume`, `MusicVolume` and
related settings, then creates `TSoundControl` and `TMusicControl`. The E2E-3
source override forces `SoundEnabled` and `MusicEnabled` to false immediately
before those constructors. `GR_Main` calls `SoundManager->UpdateFades()` during
the main loop and destroys music before sound on shutdown.

`TSoundControl::PlaySound`, `PlayEffect`, and `PlayLoop` obtain `TCSoundEC`
through the original cache. `TCSoundEC::LoadFromConfigBuffer` loads RIFF/WAVE
PCM; its present implementation assumes a 44-byte header and scans raw bytes
for `data`, without validating chunk boundaries. Each `TSoundBuffer` copies
PCM to a DirectSound secondary buffer. `SuppressGroup` limits duplicate effect
groups; `RemoveFinishedBuffers` releases finished one-shot sounds. Controllers
retain references to looping buffers and detach in `Clear`.

`TMusicControl` selects files from `Music.<category>` in the original config.
`PlayFile` queues a `TMusicUnit`; `PlayCategory` calls `PlayFile` while holding
`ControlLock`, so the lock must be recursive or that path deadlocks. A music
unit reads through `EC_FileStream` (the game calls tracks such as
`music\\intro.dat`) and `VorbisFile`, decodes in its worker thread, and refills
three DirectSound buffer chunks using position notifications. Fade changes
are driven by a WinMM periodic event. `TMusicUnit::Clear` currently leaves its
decoder and decoder library allocated in the original destructor.

The actually used DirectSound subset is device create/enumerate and
cooperative level, primary buffer format, secondary buffer creation,
`QueryInterface` for notifications, `Lock`/`Unlock`, `Play`/`Stop`, status and
play cursor, volume, pan, and three position notifications. `GR_Sound` also
uses `CreateEvent`, `SetEvent`, `ResetEvent`, single/multiple-object waits,
`timeGetTime`, `timeSetEvent`, and `timeKillEvent`. Music decoding occurs on
`TMusicUnit` workers; sound creation/control and fades occur on game threads.
The SDL audio callback must only read predecoded PCM and mix existing voices.

## Feasibility and decision

Adapter A, replacing calls at the `GR_Sound` / `GR_Music` boundary, would need
to duplicate the original cache, group suppression, looping controllers,
music selection and fade state. Adapter B, a narrow DirectSound-compatible
layer, preserves those paths but must supply the exact lock/unlock and
notification semantics. The original methods make B the intended final
integration: a platform-owned SDL2 mixer with an immutable/copy-owned PCM
voice, a bounded streaming ring, and notifications posted outside the audio
callback. This is an architectural decision, not a claim that the backend is
implemented. Until all these semantics pass tests, Audio ON must fail its
build/test gate rather than silently use Audio OFF.

The requested baseline is stereo 44.1 kHz, PCM 16 or float internally; the
device's obtained rate and format must determine conversion. An audio-device
open failure may fall back to soundless gameplay with a distinct log entry.
Runtime decode/output failure must be reported separately.

The installed Switch portlibs contain `vorbis/vorbisfile.h` and
`libvorbisfile.a` (checked locally 2026-10-11). The original 720-byte embedded
Vorbis state and 32-bit `read_func` signature are not valid assumptions for
the ARM64 C ABI. The implementation needs the native `OggVorbis_File` type
from those headers, with callback `size_t` arguments and correct element-count
return semantics. The original 4096-byte scratch buffer is also vulnerable to
an oversized `ov_read` request. Vorbis assets are identified by content, not
the `.dat` suffix; game assets are not included in this repository.

The requested FPC reference `platform/GameAudio.pas` could not be fetched in
this environment at audit time. Its SDL2 buffer and notification behavior
therefore remains a comparison task; no code has been copied from it.

## Evidence boundary

This document maps source and installed dependencies. It establishes neither
audible output nor a passing Audio ON build. Audio OFF is the E2E-3 baseline.
Host PCM tests, full ARM64 linkage and later physical Switch playback are
separate gates.
