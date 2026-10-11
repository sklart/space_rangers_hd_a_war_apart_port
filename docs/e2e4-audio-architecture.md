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

Replacing the entire `GR_Sound` / `GR_Music` layer would duplicate the cache,
group suppression, looping controllers, music selection and fade state. A full
DirectSound COM facade would preserve these callers but also require the exact
COM, lock/unlock and position-notification behavior. The experimental Switch
adapter therefore replaces only low-level `TSoundBuffer` methods in disposable
generated source. It preserves the original higher-level game code and maps
PCM voices and three-chunk streams to SDL2. Audio ON is isolated from the
normal build while its runtime behavior is tested.

The requested baseline is stereo 44.1 kHz, PCM 16 or float internally; the
device's obtained rate and format must determine conversion. An audio-device
open failure may fall back to soundless gameplay with a distinct log entry.
Runtime decode/output failure must be reported separately.

The E2E-4 tree now contains an independently tested PCM mixer and an SDL2
output wrapper. The mixer accepts mono/stereo unsigned 8-bit or signed 16-bit
PCM, resamples to the actual output rate, mixes up to 64 concurrent voices,
and supports effect/music masters, gain, pan, loop, stop and gain ramps.
Finished voices retain PCM ownership until the game thread collects them;
the callback only mixes preloaded samples. `SdlOutput` opens a paused stereo
S16 device at 44.1 kHz with frequency change allowed, constructs the mixer
for the obtained rate, then starts output. It closes the device before freeing
PCM. The experimental adapter now calls this output wrapper from the original
`TSoundBuffer` path; audible behavior still needs verification.

The installed Switch portlibs contain `vorbis/vorbisfile.h` and
`libvorbisfile.a` (checked locally 2026-10-11). The original 720-byte embedded
Vorbis state and 32-bit `read_func` signature are not valid assumptions for
the ARM64 C ABI. `e2e4_native_vorbis.cpp` now owns a genuine `OggVorbis_File`
from those headers. A pointer to the separately allocated decoder is stored
unaligned in the original worker's opaque byte array; the byte array itself
is never passed to libvorbisfile. The native callback uses `size_t` arguments
and complete-element return semantics. Decoded reads remain bounded to 4096
bytes, and each track's actual channel count and sample rate are advertised
to the original streaming buffer. The decoder is closed before its
`EC_FileStream` is released. Chained logical streams that change format are
rejected rather than played with the wrong PCM format. Ogg assets are
identified by content, not the `.dat` suffix; game assets are not included
in this repository. The experimental adapter connects this decoder's PCM to
the SDL2 streaming ring.

The requested FPC reference [`platform/GameAudio.pas`](https://github.com/pakompom/SpaceRangersHD_FPC/blob/main/platform/GameAudio.pas)
was fetched and inspected on 2026-10-11. It implements a DirectSound-shaped
interface over SDL queued audio. Its separate mixer worker posts the original
buffer position events; its SDL device has no callback. This confirms the
feasibility of preserving the original `GR_Sound` and `GR_Music` callers, but
the Switch backend uses its own callback and must deliver notifications from
outside that callback. No FPC code was copied. The FPC port's custom changes
are [MIT licensed](https://github.com/pakompom/SpaceRangersHD_FPC/blob/main/LICENSE),
with separate terms for the underlying game and dependencies.

`e2e4_audio_stream.hpp` now supplies a bounded three-chunk PCM ring, rate
conversion, silence on missing refill, and counted chunk completions.
`SdlOutput` can own two such streams. SDL's callback only mixes already decoded
PCM and records completion counts; the non-callback caller drains them. Host
ring tests passed locally; an SDL dummy-device sanitizer test is added to CI.
The component is now wired through an experimental source override for the
original `GR_Sound::TSoundBuffer` methods. The original high-level sound cache,
group suppression, looping controllers, music selection, and `TMusicUnit`
worker remain in place. `--experimental-audio-on` produces a separate NRO;
the normal build still forces Audio OFF. The bridge maps original buffer
pointers to owned PCM voices or two streaming rings. The original completion
event numbering is reconstructed outside the SDL callback. Device-open failure
turns off both sound and music with a distinct log entry.

The experimental path passed a local full ARM64 link on 2026-10-11: 298 original
units, 378 linked objects, zero unresolved symbols, ELF64 AArch64 PIE.
The resulting dirty-tree NRO SHA-256 is
`DB159D3F7537733B657EADA5818F6645A75B871ED345671E882F671C7BB83E85`.
Host SDL dummy sanitizer testing and playback on Switch are still pending.
It is not an accepted Audio ON build. The
current stream mix adds onto S16 output after effect clamping; combined loud
effects and music may clip early. That mix order must be corrected before
audible acceptance. The bridge currently applies gain linearly while the
original DirectSound path converts its volume setting to a decibel value;
perceptual volume parity still needs comparison. No original save/load or
hardware audio test has run.

## Evidence boundary

The host PCM ring and ARM64 link pass do not establish audible output or
original-game music transitions. Audio OFF is the E2E-3 baseline. Sanitized
SDL host testing, original save/load testing, and physical Switch playback
remain separate gates. Status: `SOFTWARE PARTIAL / HARDWARE PENDING`.
