# MatrixGame E583: две ошибки исходной очереди приказов робота

Статус: обе ошибки воспроизведены MSVC ASan, исправлены в generated port copy;
real-world queue/ownership oracle и Windows HAL regression PASS. Host-ASan/UBSan
и ARM64 проверены **компиляцией** изменённых TUs. Отдельная SDK NRO
compile/link/package PASS, strong undefined = 0. Switch **NOT RUN**.

## Дефекты и исправление

`CMatrixRobotAI::RemoveOrder(int)` сдвигал приказы через `memcpy`, хотя области
перекрываются. На настоящем idle-роботе загруженной карты baseline ASan выдал
**memcpy-param-overlap**: при удалении первого из трёх приказов копируются
64 bytes со сдвигом 32 bytes. Baseline закончился exit 1.

В `ProcessOrdersList` выражение `sizeof(SOrder) * count - 1` имело неправильные
скобки. Для двух активных приказов на x64 оно читает 63 bytes вместо 32;
лишние 31 bytes принадлежат неактивной ёмкости. При пяти приказах чтение
пересекает границу всего массива в следующий member робота. Последующее
присваивание последнего элемента обычно скрывает проблему в конечном порядке,
но не делает лишнее чтение допустимым.

Эта вторая ошибка воспроизведена отдельным baseline: fixture размечает границу
активной очереди `__asan_poison_memory_region`, затем вызывает настоящий
`ProcessOrdersList`. ASan обнаружил **use-after-poison, READ of size 63**.
Это специально размеченная логическая граница, а не свидетельство use-after-free
или самостоятельной способности обычного ASan обнаруживать subobject overread.
Baseline executable/source/link inputs/log сохранены отдельно от первого.

Генератор `matrixgame_actor_overrides.py` теперь создаёт:

- `memmove` ровно оставшихся приказов после удаления, с пропуском пустого хвоста;
- `memmove` ровно `(count - 1) * sizeof(m_OrdersList[0])` при ротации.

Original Release удаляемого приказа, порядок ротации, предел MAX_ORDERS=5,
остальные поля и ручное владение SObjectCore сохранены. Generated manifest
относительно E582 меняет ровно `MatrixGame/src/MatrixRobot.cpp`. Pinned upstream
не редактируется. Это исправление UB на обеих ABI, не новая игровая логика.

## Исполнение и пределы доказательства

Новый probe выбирает настоящего робота карты с пустой очередью и без base binding.
Он не подменяет объект выделенной памятью/фиктивным vtable и не исполняет
добавленные приказы как действия робота. В каждом из двух запусков проверены:

1. Все **15 комбинаций** размера 1–5 и позиции удаления, плюс invalid indices.
   Сравниваются тип/фаза, четыре параметра и object-core pointer всех приказов.
2. По десять ротаций для каждого размера 0–5 — **60 проверок** полного порядка
   и полей, с ASan logical-read boundary. Отсутствие доступа при 0/1 тоже проверено.
3. Три приказа с настоящим SObjectCore робота: GetCore/RefInc, дополнительная
   ротация, последовательные Release и возврат точного исходного refcount.
4. Исходная пустая очередь восстановлена до продолжения original карты.
5. Полный original Init/transition/four-frame/Deinit с настоящим owning Rangers
   host, Windows HAL, восстановлением gamma/window и очисткой leases/owners.

Итого два успешных oracle, восемь настоящих 800×600 PNG и полный host cleanup.
MSVC ASan инструментирует пять новых harness/actor TUs; остальной engine/Rangers
переиспользуется без полной инструментации. STL container annotations согласованы
с прежними библиотеками, address/overlap проверки остаются включены. Полный engine
ASan, LeakSanitizer и UBSan execution этим запуском не заявляются.

Изменённый полный MatrixRobot.cpp и oracle скомпилированы host ASan/UBSan и ARM64
GCC. Их новые Linux/QEMU execution **NOT RUN**. Большие уже пройденные suites
без причины не повторялись. Interactive loop E581 остаётся прежним отдельным
доказательством; текущие paused readbacks не выдаются за новый полный бой.

## Воспроизведение и артефакты

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_robot_orders_native_build.py
python tools/matrixgame_robot_orders_windows.py --baseline remove
python tools/matrixgame_robot_orders_windows.py --baseline rotate
python tools/matrixgame_robot_orders_windows.py
python tools/matrixgame_robot_orders_cross_compile.py
python tools/matrixgame_robot_orders_sdk.py
python tools/matrixgame_robot_orders_audit.py --output reports/matrixgame-e2e583-robot-orders-recheck.json
```

Две baseline команды намеренно завершаются exit 1 на сохранённой старой копии
MatrixRobot.cpp. Audit пишет новый файл exclusively. Exact commands, stdout,
ASan diagnostics, source/link fingerprints, независимая PNG-проверка и protected
hashes — [в evidence](matrixgame-e2e583-robot-orders-evidence.json).

Новая отдельная стандартная SDK NRO:
`build/matrixgame/e2e583-robot-orders/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.
Размер **14895301 bytes**, SHA256
`53db0a16a8f187fc8dd50a55be20e2e3bd3335d6f86e80c02c13e164a9664233`,
build ID `82034333146d-7f1c2b2bb3e62afaeca9`. NRO/ASET/NACP/fonts,
source/dependency fingerprints и все защищённые 78 game inputs, доступные frozen/
save artifacts, primary/cumulative NRO, logs2.7z и clean upstream pins проверены.
Target
`matrixgame-native-diagnostic-nro`, обычные devkitA64/libnx/deko3d средства.
Прежняя E582 NRO сохраняется с SHA256
`2519b5122f556cf9be1cbd8b53126e1fd8a9e50d833c68c4d763862afa2e0d5e`.
Основная NRO не изменяется. Resource roots/private output и read-only manifest
описаны [в инструкции E575](../docs/matrixgame-e2e575-hosted-diagnostic.md).

## Открытые этапы

Это новые доказанные source/memory исправления, не завершённый аудит всех
операций копирования и не hardware acceptance. Последний измеренный Switch
рубеж по прежним logs2 остаётся ReadParams/ApplyGammaRamp до map constructor.
Открыты native/deko Init/map/form/loop, image/audio/HID/F9/focus/recovery,
полноценный бой и возврат результата основной игре. Аппаратный запуск и перенос
данных выполняет пользователь. Commit/push агент не выполнял.
