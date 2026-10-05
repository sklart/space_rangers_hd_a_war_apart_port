# Switch deployment

Canonical SD layout:

```text
sdmc:/switch/space-rangers-hd-a-war-apart/
  SpaceRangersHDAWarApart.nro
  game/     original game installation, read-only and persistent
  config/   CFG.TXT
  save/
  logs/     port.log, port-prev.log and gr-main.log
  runtime/
```

Copy the complete original game directory to `game/` once. Do not update or
delete it during ordinary test iterations. Afterwards replace only
`SpaceRangersHDAWarApart.nro` (and, when intentionally needed, small files
under `config/` or `runtime/`). Game assets are never committed to this
repository.

`logs/port.log` is the canonical M10 diagnostic file. `logs/gr-main.log` is the separate translated `GR_Main::SessionLog`. Each launch moves the prior
session to `logs/port-prev.log` before creating a new log.
