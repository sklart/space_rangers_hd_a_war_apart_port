# MatrixGame E581: оригинальный form/loop с настоящим Rangers host

Статус: **Windows HAL interactive composition PASS**, два цикла Init/loop/
recovery/Deinit. **Switch NOT RUN**. Production-код в этом этапе не изменён;
новая проверка соединяет уже реализованные зависимости в исполняемый сценарий.

## Закрытый пробел проверки

E579/E580 доказали полный Windows Init с owning Rangers host и кадры карты после
loading transition, но эти кадры оставались на стартовом диалоге с паузой.
Предыдущий самостоятельный form/loop probe проверял ввод отдельно от нового
реального владельца Rangers data/audio/music/callbacks. Это не доказывало их
совместное исполнение после начала симуляции.

Новый `probe_matrixgame_hosted_windows_loop.cpp` использует настоящие
RangersHostSession, copied full16 services table, NativeInitRuntime,
CFormMatrixGame, original L3GRun и original map Takt/Draw. Только часы и события
ввода задаются детерминированным test driver. Graphics/device/window, карта,
ресурсы, AI/симуляция и host services выполняются production-кодом; GPU-операции
не подменены фиктивным успехом.

В каждом из двух запусков:

1. Настоящий Windows HAL и original Init загружают карту 276×285 (groups 28×29).
   Original loading transition завершается после 30 Takt/Draw шагов.
2. Создаётся original form и выбирается FormChange. Enter закрывает стартовый
   диалог, Pause ставит/снимает паузу, Escape открывает/закрывает меню.
3. Настоящий EngineSink передаёт MouseMove и configured scroll key. Проверены
   координаты курсора, key down/up и фактическое изменение координат камеры.
4. Полный original loop выполняет **208 Draw и 5125 мс игрового времени**.
   Сверх начальных input-событий работают 200 обычных unpaused шагов по 25 мс.
   Исходный loop выходит через GFLAG_EXITLOOP, не через подмену его тела.
5. Реальные SystemEvent focus loss/recovery освобождают video resources и
   восстанавливают их через original Takt; выполняется новый original form Draw.
6. Получены четыре последующих настоящих framebuffer readbacks 800×600.
   Их RGB hashes различаются внутри каждой сессии. После уничтожения form
   цепочка g_FormCur/First/Last пуста.
7. Original Deinit восстанавливает device/window/gamma, оставляет заимствованный
   Rangers host живым, после чего сам host полностью очищается. Нет утечек
   retained skin/draw/resource owners, device/timer leases; cwd не меняется.

Итого **416 loop Draw, 10250 мс loop simulation и 8 финальных PNG** за два
цикла. Recovery и дополнительные финальные Takt/Draw в эти loop counters не
включены. Это ограниченная проверка начала симуляции, не полный бой и не
проверка условий победы/поражения или возврата результата основной игре.

## Сборка и доказательства

Использованы read-only свежие E580 production native archives (160 actual
MSVC x64 objects), 337 проверенных Rangers objects и их настоящие библиотеки.
Добавлены семь original/native form/loop/input translation units, уже входящих
в SDK diagnostic target; они отсутствовали в минимальной Windows Init-only
closure. Первая попытка нового probe получила 11 unresolved symbols именно для
этих отсутствовавших dependencies. Лог и result сохранены. После добавления
настоящих TUs link и исполнение прошли, без заглушек или ослабления assertions.

Новый joint executable собран MSVC x64 с assertions, без DNDEBUG. Windows
resource/font, все link input hashes, source composition SHA, stdout и два
успешных diagnostic journals проверены. Независимый stdlib PNG decoder проверил
CRC/zlib, размеры, RGB bytes и различие кадров; в них 143875–144228 distinct RGB
colors. Один PNG просмотрен: карта, роботы/здания, русский интерфейс и миникарта
реально видны; стартового диалога нет.

[Machine-readable evidence](matrixgame-e2e581-hosted-windows-loop-evidence.json).
Снимок:
`build/matrixgame/e2e581-hosted-windows-loop/runtime/output/map-frame-3.png`.

Большие пройденные host ASan/UBSan, ARM64/QEMU и SDK suites **не повторялись**:
production source не изменён. Это Windows execution proof новой композиции,
а не новое sanitizer/ARM/deko execution доказательство.

## Сохранённая диагностическая NRO

Новая NRO не требовалась: изменены только test driver и test/build harness.
Все записанные compiled production inputs предыдущего SDK пакета побайтно
совпадают с текущими. Из исторических own-source entries E580 изменился ровно
один общий test runner: `tools/matrixgame_hosted_windows_map_checks.py` получил
параметры PROBE/EXTRA_SOURCES с прежними defaults. Аудит явно записывает этот
nonproduction delta; исторический fingerprint не выдаётся за fingerprint всего
текущего дерева. Новые тестовые файлы также не меняют исполнение NRO.

Текущая сохранённая NRO:
`build/matrixgame/e2e580-loading-capture/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.

- Размер **14895301 bytes**.
- SHA256 `b6ec6ce859ce21c9471084d1e63be8a45d7f492f0f98a31e0b3cb421e4ee6c8b`.
- Build ID `82034333146d-881a0f68c6186ed89122`.
- Стандартная цель `matrixgame-native-diagnostic-nro`, libnx/deko3d.
- Ранее проверенные compile/link/package PASS, strong undefined = 0.
- **Combined native/deko runtime NOT RUN**.

Основная/cumulative/прежние NRO, upstream pins, 78 game inputs, доступные
frozen/save artifacts и logs2.7z сверены read-only и сохранены. Commit/push,
перенос данных и взаимодействие со Switch не выполнялись.

## Воспроизведение и точный остаток

Из корня проекта, с существующими VS/Python dependencies и сохранёнными E580/
E577 archives:

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_hosted_windows_loop_checks.py
python tools/matrixgame_hosted_windows_loop_audit.py --output reports/matrixgame-e2e581-hosted-windows-loop-recheck.json
```

Не требуется заново запускать E527, E529, full host/ARM/SDK suites. Audit создаёт
новый JSON exclusively. Точные исходники, archives и команды — в evidence.

Подтверждённая программная цепочка на Windows теперь включает owning Rangers
data/audio/music → full16 binding → original Init/map/transition → original
form/loop → Enter/menu/pause/camera → несколько секунд unpaused simulation →
focus release/recovery → changing map frames → Deinit → host cleanup.

На Switch последний измеренный рубеж по старым logs2 остаётся
ReadParams/ApplyGammaRamp до map constructor. Native capture подключён E580,
но полный native/deko Init/map/form/loop ещё не исполнялся. Конкретный следующий
native runtime отказ без аппаратного запуска не установлен. Открыты native
graphics/readback, реальный HID/audio/focus/F9/recovery, полноценный бой и возврат
его результата основной игре. Windows HAL этот остаток не закрывает.
