# MatrixGame E580: отдельная диагностическая NRO

Новый бинарник:
`build/matrixgame/e2e580-loading-capture/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.

Размер **14895301 bytes**, SHA256
`b6ec6ce859ce21c9471084d1e63be8a45d7f492f0f98a31e0b3cb421e4ee6c8b`.
Build ID `82034333146d-881a0f68c6186ed89122`.

Стандартная SDK цель `matrixgame-native-diagnostic-nro` на devkitA64/libnx/deko3d.
Compile/link/package PASS, unresolved strong symbols = 0. **Switch NOT RUN**.
Основная и прежние NRO сохранены.

Обязательная native loading-transition ветка теперь получает настоящие pixels
завершённого кадра через DekoDevice screenshot provider. Снимок принадлежит CPU
до video Reset и однократно используется original CTransition. Отсутствие кадра,
неверные metadata или смена размеров остаются явными ошибками.

Windows HAL подтвердил owned pixels через Reset, два full original Init/map/
transition/4-frames/Deinit цикла и cleanup. ASan/UBSan и ARM64/QEMU подтвердили
адресные CPU contracts. Эти результаты не доказывают full native Init на Switch.

Ресурсные roots, private outputs, 26-entry read-only resource manifest и порядок
будущего ручного запуска — [в инструкции E575](matrixgame-e2e575-hosted-diagnostic.md).
Ресурсы и сохранения агент не переносил; устройство не использовал.

[Точные результаты, Init-цепочка, hashes и воспроизведение E580](../reports/matrixgame-e2e580-loading-capture.md).
