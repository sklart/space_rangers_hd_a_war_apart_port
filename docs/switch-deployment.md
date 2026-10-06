# Switch deployment

Canonical SD layout:

```text
<SD>/switch/space-rangers-hd-a-war-apart/
  SpaceRangersHDAWarApart.nro
  game/     original game installation, copied once and then preserved
  config/
  save/
  logs/     port.log, port-prev.log and gr-main.log
  runtime/  deployment.txt
```

`tools/deploy-switch.ps1` validates the 2.1.2500 release by the SHA-256 of
`Rangers.exe` before it can copy `game/`. It never uses `/MIR`, `/PURGE` or
`/MOVE`; ordinary updates do not traverse or copy the game tree.

```powershell
# Первый раз: укажите корень SD и исходную установленную игру.
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -NroPath .\port\switch\SpaceRangersHDAWarApart.nro -GameSource 'D:\Games\Space Rangers HD A War Apart' -InitialGameCopy

# Все следующие сборки: заменяется только NRO и runtime/deployment.txt.
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -NroPath .\port\switch\SpaceRangersHDAWarApart.nro -UpdateOnly
```

If a valid `game\Rangers.exe` already exists, a repeated initial command reports
`GAME COPY SKIPPED — existing baseline valid`. Use `-ForceGameCopy` only when an
intentional full recopy is required. To retrieve diagnostics without modifying
SD contents:

```powershell
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -CollectLogs
```

It writes available `port.log`, `port-prev.log` and `gr-main.log` into a local
timestamped `hardware-logs/` directory. Each deploy concludes with the compact
preflight report and `READY FOR SWITCH LAUNCH`; failure reports `BLOCKED` and a
nonzero exit code.
