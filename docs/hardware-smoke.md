# M11C Switch hardware smoke

## Deploy

Use the deployment helper rather than manually copying individual release
files. The first command validates the known release baseline and copies the
complete original game once; later commands replace only the NRO.

```powershell
# Первый раз
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -NroPath .\port\switch\Space Rangers HD - A War Apart.nro -GameSource 'D:\Games\Space Rangers HD A War Apart' -InitialGameCopy

# Все следующие сборки
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -NroPath .\port\switch\Space Rangers HD - A War Apart.nro -UpdateOnly
```

Pass the SD filesystem root (for example `E:\`) as `SdRoot`; nested folders are
intentionally rejected. The preflight must end with `READY FOR SWITCH LAUNCH`.
It requires the checked `Rangers.exe`, all M9 release files, equal NRO
source/destination SHA-256 and all writable directories. A failure is a
deployment blocker, not a reason to modify runtime code.

## First launch evidence

After launch, collect logs without touching SD content:

```powershell
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -CollectLogs
```

`CollectLogs` is only a PASS when it copies at least one recognized log; missing
app root, `logs/`, or all logs is a nonzero failure. Inspect `[BOOT]`, `[M9]`,
`[M11] GlobalCache PASS`, `[M11] resource`, `[M8]`, package baseline and
`[BOOT] COMPLETE`. Hardware observations remain separate from host and
cross-build evidence.

The local `tests/test_deploy_switch.ps1` deployment regression passed. After a
successful preflight, the NRO is ready for the first Switch launch.

## M12 Switch runtime test

The M12 NRO must remain visible after startup. Confirm that the heartbeat changes, wait at least 15 seconds, then press `PLUS`. `port.log` must show `runtime ready`, `entering persistent loop`, `exit_reason=plus`, a nonzero frame/present summary, orderly shutdown and `[BOOT] COMPLETE`. Do not treat hbmenu Home handling as the M12 exit test.

## Recorded M12 result

**PASS** - build `63e1f5f`, 1,574 frames/presents over 78,817 ms, `PLUS` exit and `[BOOT] COMPLETE`.

## Recorded M14P hardware result

**PASS** - tested commit and embedded `build_git` `46330b4`. The real GI key `Bm.Captain.2BlazerBi` (`data\\Captain\\2BlazerB.gi`) decoded as Format 0 RGB565 to a 93x104 BGRA CPU image with pitch 372, CRC32 `cf5b1d56`, and FNV-1a `a668e341bc42a6fb`. M13 metadata passed, then the M12 loop ran for 1,506 frames/presents over 73,472 ms, exited through `PLUS`, and reached `[BOOT] COMPLETE`.

The tested `Space Rangers HD - A War Apart.nro` was 7,158,064 bytes with SHA-256 `1CDFFC96A8505BA6D9114222F46CF32B4A16E80C93C93735D13198293E61D97E`. `port.log` confirms embedded `build_git`; the deploy preflight/manifest confirms the NRO SHA-256.

## Recorded M15 hardware result

**PASS** — tested commit and embedded `build_git` `9d8d8bb`; NRO SHA-256 `EC4B1936E2CC7C0E39EBB9DF20FCD60979168BD05D283DDF3C91C5437C695D01`. After M14P, the fixed raw frame 0 of `DATA/BGObj/bg00.gai` passed container validation, GI extraction and Format-0 RGB565 CPU decode. Its GAI CRC32/FNV-1a is `9e05776f`/`03f332f6307d4031`, GI CRC32/FNV-1a `05d665d2`/`3ccdac34b2d0a2cc`, and decoded 2000x2000 BGRA CRC32/FNV-1a `3fc81562`/`ad9d67c6c7ad85b9`. The diagnostic then entered M12, presented 2,003 frames over 100,264 ms, exited through `PLUS`, shut down cleanly and reached `[BOOT] COMPLETE`.

## M15 first hardware attempt

**NOT PASS.** NRO `73bc577` reached `DecodeGaiFormat0Frame` for the fixed real frame and logged matching GAI, GI and pixel CRC32 values, but emitted `[STAGE] M15 GAI FAIL unknown`. The reported FNV-1a values revealed a truncated offset-basis literal in the runtime, whereas the independent release oracle uses the standard 64-bit value. The NRO must be replaced with the correction and retested; this run is not evidence for M15 PASS.

## M16 Switch test

**PASS.** The tested `Space Rangers HD - A War Apart.nro` SHA-256 is `470246273738C66777672E0D88A3449DA3FF47398AC08C16BB3175B5CAE8E08E`, with `build_git=50b778c`. The log records `[STAGE] M16 GI format2 PASS`, all 100 frames, frame 0 CRC32/FNV `83f66519`/`eb000366ca288b23` and aggregate CRC32/FNV `9e4059ce`/`a028ffbf04472afa`. M12 then ran 943 frames/presents for 47,272 ms, exited with `PLUS`, and reached `[BOOT] COMPLETE`.

## Recorded M17 Switch result

**PASS.** The tested NRO was 7,194,928 bytes with SHA-256 `E753BFEAA6BF71C48086C98BB8A27307A0929ECB05B362D343CFA7B8777DCD0D` and embedded `build_git=13ad513`. `DATA/Asteroid/00.gai`, sequence 0, reported 100 positions with 50 ms delays, a 5000 ms nominal cycle, and matching sequence CRC32/FNV-1a `4b1c6ebf`/`47cdc8c73fc1ce61`. The first decoded cycle matched CRC32/FNV-1a `5b7bc7e9`/`f70813ac799a25b3`; its actual duration was 5033 ms, max tick gap 129 ms, and measured tolerance 134 ms. After `[STAGE] M17 GAI playback PASS`, M12 ran 832 frames/presents for 41,746 ms, exited through `PLUS`, and reached `[BOOT] COMPLETE`.

For a re-test, deploy a hash-verified NRO with `-UpdateOnly`, wait at least 15 seconds without pressing `PLUS`, then collect both logs after `PLUS`. A valid run must still contain the M17 fingerprints, `[M17] PASS sequence=0 cycle=1`, `[STAGE] M17 GAI playback PASS`, nonzero frames/presents and `[BOOT] COMPLETE`.

## Recorded M18 Switch result

**PASS.** The tested 7,199,024-byte NRO had SHA-256 `D2322AB4881796FFE3EB55CF493A6191A54547B636EBA10C24EC3FBF91BE9DDB` and embedded `build_git=f9833a5`. `port.log` records `DATA/Asteroid/00.gai`, sequence 0 / source frame 0, decoded 33x40 BGRA with pitch 132 and CRC32/FNV `83f66519`/`eb000366ca288b23`, then centred destination 623,340 in the 1280x720 RGB565 framebuffer and `[M18] compositor PASS`. The supplied Switch screenshot shows the real Asteroid visibly composited over the moving M12 heartbeat. M17 still matched its cycle CRC/FNV `5b7bc7e9`/`f70813ac799a25b3`; its first cycle took 5042 ms with max tick gap 140 ms and tolerance 145 ms. M12 ran 3,331 frames/presents for 166,725 ms, exited through `PLUS`, and reached `[BOOT] COMPLETE`.

## M19 Switch test

Deploy only the NRO whose `port.log` reports the new committed `build_git`; an old `f9833a5` log is M18 evidence only. Leave the scene visible for at least 10 seconds, then press `PLUS` and collect both logs. A M19 PASS requires `[M19] scene compositor BEGIN`, exactly three logged sprites (Asteroid, deterministic second GAI and alpha overlay), `scene_crc32`/`scene_fnv64`, `[M19] scene PASS`, retained M17 cycle evidence, nonzero frames and presents, `exit_reason=plus`, and `[BOOT] COMPLETE`. Capture a screenshot where more than one real resource is visible.

## Recorded M19 Switch result

**HARDWARE PASS.** The tested NRO was 7,219,504 bytes, SHA-256 `E05B83F0675548D25911AC497541EDFF1C8D0EE66CD5FD26922FA9D57EE131FF`, embedded `build_git=3b337cd`. It logged `sprites=3`: Asteroid frame 0 at 583,340 (layer 0), lexical second resource `DATA/Asteroid/01.gai` frame 0 at 664,347 (layer 10), and the 50%-alpha Asteroid overlay at 593,350 (layer 20). The canonical scene fingerprint is CRC32 `ba977214`, FNV64 `240b58539a257627`, canonical bytes 14,244. The supplied screenshot visibly shows both real resources. M19 reported PASS; M17 retained its matching 5,040 ms cycle, and M12 ran 594 frames/presents for 29,993 ms before `PLUS` and `[BOOT] COMPLETE`. The independent Python oracle against the local release tree selected the same sprites and exactly matched this scene fingerprint. CI `37745110922` passed the M19 regression and symbol audit, completing the M19 gates.

## M20 Switch test

Deploy only the committed M20 NRO whose startup log reports its new `build_git`. Keep the three objects visible for 30–60 seconds, then press `PLUS`; do not use a screenshot or an old M19 log as proof. A valid M20 result requires `[M20] GI object BEGIN`, `objects=3`, three object lines with resource/frame/position/layer, `scene_crc32`/`scene_fnv64`, `[M20] GI object PASS`, retained M17 cycle evidence, nonzero M12 frames/presents, `exit_reason=plus` and `[BOOT] COMPLETE`. A screenshot of multiple real resources and the alpha overlay is useful visual evidence but is not a gate when the complete log is supplied.

**HARDWARE PASS.** The tested NRO embedded `build_git=73340fc` and SHA-256 `0F7B909287B28B4D6DD7CE3617DF73E869907ED9F328F9E6A71AF81970523243`. It logged `DATA/Asteroid/00.gai`, `01.gai`, and a 50%-alpha `02.gai` overlay at layers 0/10/20; scene CRC32/FNV64 was `462de41f`/`694d6066b47b99a5`. M20 and the retained M17 cycle passed; M12 produced 3,824 frames/presents in 172,463 ms, exited through `PLUS`, and reached `[BOOT] COMPLETE`. CI `37753205358` passed the M20 regression and symbol audit.

## Recorded M21 Switch result

**HARDWARE PASS.** The one final NRO was 7,547,184 bytes, SHA-256
`6D387610DAD7DA57C1D9F21F00ABF84C67F9AA266421CF54C98567656C8FA3F1`, with
embedded `build_git=fc8adfc`. It logged the deterministic real Simple
`DATA/Planet/Spu00.png` at 128×60, source CRC/FNV `a3721a9c`/
`32ebfdd05d7fa674` and decoded RGB565 CRC/FNV `51e16db2`/
`ffeaf550d3c28655`. The release inventory honestly reported Trans and Alpha
as `NOT_PRESENT`; no arbitrary resource was substituted.

The integrated M20/M21 RGB565 checkpoint matched the independent oracle:
actual and expected CRC32 `4f915772`, FNV64 `52449ae8f8f56c6c`, 1,843,200
bytes. M20 GIObject and M21 both passed; retained M17 completed sequence 0 in
5,008 ms. The persistent loop produced 3,824 frames/presents over 172,747 ms,
then `PLUS` caused a clean shutdown and `[BOOT] COMPLETE`. `gr-main.log`
contains only its expected `Start` marker. The screenshot is supplementary
visual evidence, not a required hardware criterion. CI `37770073920` had
already passed before this one physical test.
