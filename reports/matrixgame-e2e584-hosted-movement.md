# MatrixGame E584: штатная команда движения и оригинальная симуляция

Статус: два Windows HAL hosted loop цикла с настоящим движением робота PASS.
Production-код не менялся. Существующая E583 NRO сохранена; Switch **NOT RUN**.

## Новый проверенный участок

После E583 очередь приказов была проверена адресным ASan oracle, но это не
доказывало исполнение приказа оригинальной симуляцией. Новый сценарий использует
те же свежие production archives и owning Rangers host, затем проходит путь:

`original Init → loading transition → original form/L3GRun → Enter → original
PlaceFindNear → RobotToLogicGroup → PGOrderMoveTo → Takt/navigation/movement →
visible map frames → focus recovery → Deinit → host cleanup`.

Тест выбирает настоящего живого робота игрока, не связанного с базой. Original
PlaceFindNear подбирает допустимую точку с учётом chassis/footprint. Затем original
side API формирует группу и получает команду движения. Это программная команда
через настоящий игровой API; mouse hit-testing и физический HID данным этапом
не проверяются. Координаты робота и его состояние тест не присваивает.

Собственный SObjectCore reference держит только observer lifetime и снимается
до original Deinit. Основной host, copied services table, устройство и ресурсы
не заменяются fixture-успехом. Часы и события Enter/Pause/menu/scroll задаются
детерминированным драйвером, как в E581.

## Фактическое движение

Обе независимые Init-сессии дали одинаковые наблюдения:

| Метрика | Значение |
| --- | --- |
| Player group | 6 |
| Начальная клетка | (192, 473) |
| Запрошенная точка | (202, 473) |
| Итоговая клетка | (201, 471) |
| Начальные world coordinates | (1938.006348, 4752.156250) |
| Изменение world coordinates | (+91.993652, −22.156250) |
| Полное смещение | 94.624159 world units |
| Время выполнения команды | 5000 мс симуляции |
| Квадрат расстояния до запрошенной точки | 100 → 5 клеток² |
| Приказов в очереди в конце наблюдения | 0 |

Это доказывает реальное перемещение в сторону цели и наблюдаемую пустую очередь.
Точное совпадение с запрошенной клеткой не заявляется: оригинальный group/place
алгоритм выбирает допустимое место. Тест не ослаблял ожидаемые условия: требуется
не менее одной move-cell world scale, изменение grid coordinates, конечное
сокращение расстояния, живой робот и ровно 5000 мс симуляции.

В каждой сессии original loop выполнил 208 Draw и 5125 мс игрового времени,
включая первоначальный Enter/Pause/menu/scroll сценарий. Команда выдана на 125 мс
и проверена на 5125 мс. После loop прошли focus release/recreate и ещё четыре
original map frames. Всего восемь readback PNG 800×600 с различающимися внутри
каждой сессии RGB hashes, 144117–144471 distinct RGB colors. Один кадр просмотрен:
робот сместился относительно исходной группы, карта/UI/мини-карта реально видны.

Original Deinit/host cleanup, gamma/window/viewport restoration, пустые device/
timer leases и неизменный cwd проверены в двух сессиях. Новая проверка не
исполняла полный бой, атаку/урон, победу/поражение или возврат результата хозяину.

## Доказательства и сохранённая сборка

Независимый audit заново вычисляет оба расстояния из grid coordinates и длину
смещения из world deltas, проверяет timestamps/queue size, совпадение наблюдений
двух сессий, PNG CRC/zlib/RGB payloads, successful journals и все source/link
hashes. Exact commands/stdout и наблюдения сохранены в
[evidence](matrixgame-e2e584-hosted-movement-evidence.json).

Новые файлы — test observer/driver/audit. Все записанные production/own inputs
пакета E583 совпадают с текущими; compiled source не менялся, поэтому SDK rebuild
и большие host-ASan/ARM64/SDK suites не повторялись. Это новое Windows runtime
доказательство совместной композиции, не новый sanitizer/ARM/deko runtime PASS.

Текущая отдельная NRO остаётся:
`build/matrixgame/e2e583-robot-orders/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.

- **14895301 bytes**.
- SHA256 `53db0a16a8f187fc8dd50a55be20e2e3bd3335d6f86e80c02c13e164a9664233`.
- Build ID `82034333146d-7f1c2b2bb3e62afaeca9`.
- Стандартная цель `matrixgame-native-diagnostic-nro`, devkitA64/libnx/deko3d.
- E583 compile/link/package PASS, strong undefined = 0; NRO **NOT RUN**.

Основная/cumulative/прежние NRO, clean upstream pins, 78 game inputs, доступные
frozen/save artifacts и logs2.7z проверены read-only и сохранены. Commit/push,
перенос данных и взаимодействие с устройством не выполнялись.

## Воспроизведение и остаток

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_hosted_movement_checks.py
python tools/matrixgame_hosted_movement_audit.py --output reports/matrixgame-e2e584-hosted-movement-recheck.json
```

Нужны сохранённые E583 native archives, E577 Windows Rangers dependencies и
прежние VS/Python dependencies. Audit пишет новый файл exclusively.

Последняя измеренная Switch граница по logs2 по-прежнему ReadParams/
ApplyGammaRamp до map constructor. Без устройства не установлена следующая
реальная native/deko точка отказа. Native Init/map/form/loop, physical HID,
image/audio/F9/focus/recovery и полноценный бой/результат/возврат остаются открыты.
Полученное Windows движение не заменяет эти доказательства.
