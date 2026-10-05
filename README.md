# Space Rangers HD: A War Apart — Nintendo Switch

Неофициальный homebrew/source port для Nintendo Switch (ARM64, libnx, SDL2). Эталон поведения — Steam/GOG release от 2025-10-13: `Rangers.exe` SHA-256 `83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.

Игровые assets и оригинальные binaries в репозиторий не входят. Нужна собственная установленная копия игры. Клонирование: `git clone --recursive https://github.com/sklart/space_rangers_hd_a_war_apart_port.git`.

Зависимости: pinned `SpaceRangersHD_CPP`, `okgf`, devkitPro/devkitA64, libnx, SDL2, JPEG, PNG и zlib. В devkitPro MSYS: `cd port/switch && make -f Makefile`. На SD: `sdmc:/switch/space-rangers-hd-a-war-apart/SpaceRangersHDAWarApart.nro` и пользовательская игра в `game/` рядом.

Текущий статус — Milestone 3: package filesystem в работе. См. [current status](docs/current-status.md).
