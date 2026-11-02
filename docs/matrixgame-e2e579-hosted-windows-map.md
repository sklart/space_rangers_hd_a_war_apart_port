# MatrixGame E579: separate diagnostic build и Windows reference

Текущая отдельная NRO:
`build/matrixgame/e2e579-hosted-windows-map/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.

Размер **14891205 bytes**. SHA256
`020b6d3debff52d5ae1efb757b2aee10b6e9886692d83a2d2d8abac2cabf0474`.
Стандартная цель `matrixgame-native-diagnostic-nro`, devkitA64/libnx/deko3d,
обычный SDK package. Основная NRO и прежняя E575 сохранены.

В неё включены original texture class factory для full Init и два 64-bit
pointer-queue исправления. Windows reference с реальным Rangers host прошёл
два полных Init/map и по четыре visible frames после настоящего loading
transition; original Deinit и host cleanup прошли в обеих сессиях.

Эта NRO **не запускалась на Switch**. Native transition capture ещё явно
Unsupported: standalone host binding впервые делает обязательной эту ветку
original Init. Следующий программный этап — настоящий framebuffer capture,
без пропуска или замены transition выдуманным изображением. Поэтому данный
artifact не объявляется готовым к игре или полному native Init PASS.

Все resource roots/private output paths, read-only 26-entry resource plan и
условия обычного ручного diagnostic запуска остаются как в
[инструкции E575](matrixgame-e2e575-hosted-diagnostic.md). Данные не переносились;
устройством агент не управлял. Не требуется переносить saves/settings.

[Дефекты, проверки, воспроизведение и точная Init-цепочка](../reports/matrixgame-e2e579-hosted-windows-map.md).
