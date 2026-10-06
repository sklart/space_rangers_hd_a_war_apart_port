# M11C Switch hardware smoke

## Deploy

Use the deployment helper rather than manually copying individual release
files. The first command validates the known release baseline and copies the
complete original game once; later commands replace only the NRO.

```powershell
# Первый раз
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -NroPath .\port\switch\SpaceRangersHDAWarApart.nro -GameSource 'D:\Games\Space Rangers HD A War Apart' -InitialGameCopy

# Все следующие сборки
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -NroPath .\port\switch\SpaceRangersHDAWarApart.nro -UpdateOnly
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
