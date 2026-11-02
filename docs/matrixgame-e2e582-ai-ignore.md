# MatrixGame E582: отдельная диагностическая NRO

Бинарник:
`build/matrixgame/e2e582-ai-ignore/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.

Размер **14895301 bytes**. SHA256:
`2519b5122f556cf9be1cbd8b53126e1fd8a9e50d833c68c4d763862afa2e0d5e`.
Build ID `82034333146d-d694ae629dae07811ef3`.

Стандартная цель `matrixgame-native-diagnostic-nro` на devkitA64/libnx/deko3d,
compile/link/package PASS, strong undefined = 0. Исправлены воспроизведённый
MSVC ASan overlap в original AI ignore FIFO и пропущенная метка новой цели.
Native loading snapshot E580 и owning Rangers host сохранены.

Основная NRO и прежняя E580 не изменены. **Switch NOT RUN**: успешная сборка
не подтверждает full native Init/map/loop или полноценный бой.

Ресурсные roots, private output и read-only manifest для будущего ручного теста:
[инструкция E575](matrixgame-e2e575-hosted-diagnostic.md).
Устройство, SD, игровые ресурсы и saves агент не изменял.

[Baseline, исправление, проверки и воспроизведение E582](../reports/matrixgame-e2e582-ai-ignore.md).
