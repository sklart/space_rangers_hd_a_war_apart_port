# MatrixGame E583: отдельная диагностическая NRO

Бинарник:
`build/matrixgame/e2e583-robot-orders/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.

Размер **14895301 bytes**. SHA256:
`53db0a16a8f187fc8dd50a55be20e2e3bd3335d6f86e80c02c13e164a9664233`.
Build ID `82034333146d-7f1c2b2bb3e62afaeca9`.

Стандартная цель `matrixgame-native-diagnostic-nro`, devkitA64/libnx/deko3d,
compile/link/package PASS, strong undefined = 0. Исправлены два воспроизведённых
дефекта очереди приказов original CMatrixRobotAI: overlap при удалении и лишнее
чтение при ротации. Native loading snapshot и owning Rangers host сохранены.

Основная и прежняя E582 NRO не изменены. **Switch NOT RUN**; аппаратный full
Init/map/loop и полноценный бой остаются открытыми.

Ресурсные roots/private output и read-only manifest для будущего ручного теста:
[инструкция E575](matrixgame-e2e575-hosted-diagnostic.md).
Устройство, SD, ресурсы и saves агент не изменял.

[Baseline, исправления, точные проверки и воспроизведение E583](../reports/matrixgame-e2e583-robot-orders.md).
