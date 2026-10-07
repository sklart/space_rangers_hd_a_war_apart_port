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

## M15 planned hardware evidence

M15 runs after M14P and before the persistent M12 loop. The expected log has `[STAGE] M15 GAI BEGIN`, the fixed `DATA/BGObj/bg00.gai` frame-0 metadata/fingerprints, and `[STAGE] M15 GAI PASS`; then the existing M12 heartbeat, frames/presents, `PLUS` exit, and `[BOOT] COMPLETE` must still be observed. Until the corrected NRO produces this sequence, M15 is **NOT PASS on hardware**.

## M15 first hardware attempt

**NOT PASS.** NRO `73bc577` reached `DecodeGaiFormat0Frame` for the fixed real frame and logged matching GAI, GI and pixel CRC32 values, but emitted `[STAGE] M15 GAI FAIL unknown`. The reported FNV-1a values revealed a truncated offset-basis literal in the runtime, whereas the independent release oracle uses the standard 64-bit value. The NRO must be replaced with the correction and retested; this run is not evidence for M15 PASS.
