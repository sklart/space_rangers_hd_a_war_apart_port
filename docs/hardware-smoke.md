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

## M18 Switch test (pending)

Deploy the newly built NRO with `-UpdateOnly`, verify the preflight SHA-256 and launch it. A small real Asteroid image must be visible in the centre over the changing M12 heartbeat; do not press `PLUS` for at least 15 seconds. Then press `PLUS` and collect both logs. The result is PASS only if `port.log` contains `[M18] compositor BEGIN`, `resource=DATA/Asteroid/00.gai`, sequence/frame/decode/coordinate diagnostics, `[M18] compositor PASS`, `[STAGE] M18 compositor PASS`, the pre-existing M17 sequence and cycle PASS evidence, nonzero frames/presents, `exit_reason=plus` and `[BOOT] COMPLETE`. A host/CI/ARM64 result alone is not hardware evidence.
