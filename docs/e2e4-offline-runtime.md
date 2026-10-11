# E2E-4 offline runtime status

Base: `codex/e2e3-preflight` at
`4210fd5d3852d50bc336ec0a68315ecd35f19a2c`.
Work branch: `codex/e2e4-offline-runtime-audio` in a separate worktree.
Pinned submodules: C++ `57fa689c630193a66fdea6ca4c79a188814991cd`,
okgf `c01aa7a168a6f1772541501074b7bba1b96550ef`.
The upstream C++ checkout was not edited.

**Overall status: SOFTWARE PARTIAL / HARDWARE PENDING.** A full Audio OFF
ARM64 link is a build result, not proof of gameplay, sound or music. The SDL2
mixer and device wrapper are not connected to the original sound/music
controllers. Audio ON has no qualified NRO.

| Subsystem | Host | ARM64 | Hardware |
|---|---|---|---|
| Sound effects | WAV parser and PCM mixer tests PASS; original `TSoundBuffer` integration NOT VERIFIED | Parser/mixer/device objects compile; Audio ON runtime NOT VERIFIED | PENDING |
| Background music | Bounded read test PASS; native Vorbis decode and category playback NOT VERIFIED | Original decoder object compiles; ABI-safe decode and Audio ON runtime NOT VERIFIED | PENDING |
| Music transitions | NOT VERIFIED | NOT VERIFIED | PENDING |
| Scripts | UI cache high-address regression PASS; live pointer-valued script cells remain unsafe | Patched `aScript.cpp` object compiles; whole scripts NOT VERIFIED | PENDING |
| Galaxy generation | Existing E2E-3 transition tests PASS; resource-backed generation NOT VERIFIED | Full original engine links; runtime generation NOT VERIFIED | PENDING |
| Protected-state XOR | Synthetic native-width transform and layout tests PASS; real galaxy round-trip NOT VERIFIED | Patched original `aGalaxy.cpp` compiles and links | PENDING |
| Save/load | Replacement candidate failure tests PASS; original saver unchanged; whole save/load NOT VERIFIED | Original saver links; runtime save/load NOT VERIFIED | PENDING |
| UI and input | Existing asset-free input/state tests PASS; real menu action NOT VERIFIED | Original main loop and input path link | PENDING |
| Performance and memory | Baseline measurements NOT AVAILABLE | Release build only; FPS/audio callback/memory behavior NOT MEASURED | PENDING |

## Changes and evidence

- `docs/e2e4-audio-architecture.md` maps original creation, lifecycle,
  DirectSound, WinMM, PCM cache and music category paths. The FPC `GameAudio.pas`
  URL was unavailable from this environment; no reference code was copied.
- `e2e4_wav.hpp` parses RIFF chunks with bounds and alignment checks, replacing
  the original fixed-header / raw `data` search in a disposable
  `EC_CacheSound.cpp` override. `e2e4_vorbis_read.hpp` bounds each decode
  request and fixes complete-element callback returns in the disposable
  `VorbisFile.cpp` override. Tests use malformed and oversize inputs.
- `e2e4_audio_mixer.hpp` numerically tests concurrent PCM voices, channel
  conversion, resampling, clipping, independent masters, pan, loop, stop and
  fades. `e2e4_sdl_audio.cpp` opens and closes an SDL2 output device using its
  obtained rate, and compiles for ARM64. Neither is invoked by the original
  `GR_Sound`/`GR_Music` path. The original embedded Vorbis state and callback
  ABI still require an ARM64-safe replacement.
- The disposable `aGalaxy.cpp` override keeps the original XOR sequence but
  walks native-width object and excluded-pointer ranges. Synthetic double-XOR
  and layout tests pass; a real generated galaxy has not been exercised.
- The disposable `aScript.cpp` override replaces the UI cache's truncated
  pointer-order key with a native-width key. A high-address regression and
  isolated ARM64 source compilation pass. Script `vkDword` pointer storage and
  serialization remain open and are a potential new-game blocker.
- `e2e4_save_replace.hpp` and its host test exercise non-destructive backup,
  install and rollback failure paths. They are **test-only**. The original
  `TSaver_Execute` still deletes/renames quicksave slots and does not check
  the final move. Automatic approval review rejected a proposed integration
  because that path can irreversibly lose user saves. No original saver behavior
  was changed.
- `tools/e2e4_log_analyzer.py` has a self-test and distinguishes PASS, FAIL,
  NOT REACHED, NOT VERIFIED and HARDWARE PENDING. It does not manufacture
  runtime evidence.

`docs/e2e4-audio-tests.md`, `docs/e2e4-gameplay-safety.md`,
`docs/e2e4-save-load.md`, `docs/pointer-width-audit.md` and
`docs/e2e4-hardware-smoke.md` contain component-level details and the future
console runbook.

| Commit | Scoped result |
|---|---|
| `6c33f8b` | Original audio path audit and chosen integration design. |
| `56e9cb9` | WAV/Vorbis read bounds and native protected-state ranges, with host tests. |
| `5483f77` | Effective Win32 manifest for those source overrides. |
| `c1c118c` | Test-only non-destructive save replacement candidate and failure tests. |
| `36f667e` | PCM mixer, SDL2 output wrapper and numerical mixer tests. |
| `0a0013a` | Evidence-aware log analyzer and physical Switch smoke plan. |
| `b6699a5` | Effective Win32 manifest for new platform audio files. |
| `db59a8d` | Native-width script UI cache keys and high-address regression. |
| `27d1b92` | Effective Win32 manifest for the script cache override. |

## Build and CI provenance

The Audio OFF NRO built from clean source revision
`27d1b9221a3fb06a896a8ae45104e78915e1b7a5` is
`port/switch/build/e2e-game/SpaceRangersHDAWarApartE2E-27d1b92.nro`,
16,030,000 bytes, SHA-256
`CB2C999F3D0C1618D8ECEEFC3E636734505CAB2DCA2CC61A69C324F32028870B`.
The hash was checked against the actual file. `e2e-build-report.json` records
the same values: 298 compiled original C++ files, 376 link objects, zero
undefined symbols, full ARM64 link PASS and `build_dirty=false`. Verify the
file again before transfer.
The report also records required symbols, the effective Win32 import surface
and first failure. The `nro.status` field means the
build audit passed; it does not mean E2E-4 gameplay acceptance passed.

The earlier E2E-2 and E2E-3 NROs were rehashed without modification:

| Build | SHA-256 |
|---|---|
| `SpaceRangersHDAWarApartE2E-0eb7380.nro` | `EA33173E0D5898EFEAA2D1AF7CF27B125DB9664738B8E6D8D5A59E20AA419F5F` |
| `SpaceRangersHDAWarApartE2E-b34b7bf.nro` | `6A5290EBBDCD69D1E093285DF2C8DA3D83410658239088AC62E4FF72F087E7B8` |

The GitHub Actions run for the pre-script-cache source (`b6699a5`) passed
`e2e4-sanitizers`, `host-regression` and `e2e-full-arm64-link`. The final
source revision requires its own CI result. Local MSYS2 lacks ASan/UBSan
runtime libraries, so sanitizer evidence comes from the Linux CI job.

## Outstanding acceptance gates

Implement and test an ABI-safe native Vorbis decoder and connect the SDL2
backend to the original sound/music controllers, including event and timer
semantics. Resolve live pointer-valued script cells without changing the
serialized 32-bit IDs. Exercise resource-backed generation, error retry,
original save/load and Extended numeric compatibility. Integrate save
replacement only after the destructive quicksave path is explicitly approved
and covered by whole-game failure tests. Capture Audio ON/OFF performance and
memory measurements, then run the hardware plan on a physical Switch.

No claim of `SOFTWARE COMPLETE` or hardware PASS is made here.
