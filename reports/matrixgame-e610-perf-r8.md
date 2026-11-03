# MatrixGame E610 r8 — ограниченный reuse и цена telemetry

Основание: [аппаратный профиль r7](matrixgame-e610-r7-hardware-20261103.md).
Interactive: 48 переходных кадров за 121.790 с, 0.394 FPS. Batch scopes внутри
loop заняли только 8.226 с; эти данные не доказывают единственный bottleneck.
Без Switch нельзя измерить NV mapping latency или объявить достигнутый FPS.

## Адресные оптимизации

1. CompletedBlockPool: фиксированные 128 slots, максимум 16 МиБ free blocks,
только exact-size reuse с прежними CPU/GPU flags. NativeDrawUpload и material
uniform packet остаются unique owners до SubmitAndWait. ReleaseCompleted
возвращает их в пул только после успешного completion. Failed wait сохраняет
owners; смена scene может заменить старые size classes в bounded pool.
В пул никогда не попадают texture/code/image/command-memory blocks.
При teardown pool очищается после wait либо закрытия failed queue. Он может
содержать дополнительно до 128 free blocks поверх 128 live packet blocks;
лимит draw packets=64 и byte quota=256 МиБ не повышались.

2. Successful batch scopes больше не делают два синхронных journal flush на
каждый batch. Ошибки queue отдельно записываются, debug SDK callback сохранён.
Сотни повторов pressure с шестью метриками заменены первыми четырьмя и powers
of two. Это только sampling телеметрии: allocation/eviction/throw исполняются
как прежде. Счётчик всех pressure events, allocations, hits/misses/free bytes
и диагностические boundaries сохраняются. fflush+fsync оставшихся records и
проверка I/O failures не отключались.

3. Агрегированные steady_clock profile counters: draw_cpu, vertex_decode,
block_create, residency, command_encode, batch_wait, journal calls/bytes/us.
Последний измеряет настоящее write+flush. Draw CPU и его вложенные категории
перекрываются; не складывать их. Новые counters на loading frames 1..4 и 30*n
позволяют отделить Init от последующего loop даже при незавершённом transition.

## Проверка

Windows MSVC, host ASan/UBSan/LSan, ARM64/QEMU UBSan: pool/data/order/retirement,
original Init handoff, journal failure/concurrency/unicode tests — PASS.
Реальные production pool/batch/frame/state code, явно CPU-model blocks/queue.
На сохранённых 92 Windows capture rows сохранены все 208808 draw calls и
3310 submissions. CPU block model: 417616 allocations baseline → 128 initial
allocations и 417488 reuse hits, 1310720 retained bytes; exact payload/order и
отсутствие reuse до completion проверены. Это не реальная GPU allocation latency.
Точные fragment/fog/raster/vertex bytes и r6 uniform offsets сохранены.
Журнал: durable nesting, exception/result, 200 concurrent records, 7 I/O fault
cases, cleanup — PASS. Первичная sampled host replay попытка содержала CRLF
в operation name; parser исправлен, окончательный benchmark выполняется с
точным oracle 7747 → 671, baseline и sampled последовательно в порядке ABBA.

Полные Windows бои не повторялись. Upstream/generated original/game resources,
сохранения, frozen, основная NRO и прошлые пакеты не меняются.
[Инструкция](../docs/matrixgame-e610-perf-r8-readme.ru.md).

## Границы

R8 HARDWARE PENDING. Уменьшение CPU-model allocations и host journal IO не
доказывает оптимальный Switch FPS; новые aggregate counters нужны для
аппаратной проверки оставшихся затрат. Таймауты, шаг игрового времени,
критерий четырёх map frames и transition не ослаблены. Campaign/Gov/естественная
победа/возврат Galaxy не охвачены standalone entry.

## Host IO профиль

Окончательный последовательный ABBA replay 7747 событий Interactive r7,
реальный fwrite/fflush/fsync на host filesystem, два прогона каждого режима.
Baseline: 13.598 / 15.289 с; sampled: 1.355 / 1.272 с. Медианы: 14.443352 →
1.313508 с, 7747 → 671 records, все исходные 591 pressure events учтены
счётчиком, в подробности попали 11. В этом replay сравнивается только политика
горячей телеметрии; новые периодические profile counters NRO здесь не
добавляются. Это не microSD benchmark и не прогноз ускорения FPS.
Source capture SHA256: `733ff6e6d1a75322429529b151a0d4b5771a1c252dca0543d9f9448b0b5d4d35`.
Результат: `build/matrixgame/e610-perf-r8-checks/benchmark/1/result.json`.

## Проверенная сборка и доставка

Source commit: `470184466ed857bd1e67b2072ce0ead6d6f22356`; build ID `470184466ed8-2a6f525547eec077fa9f`.
ZIP: `dist\matrixgame-e610-switch-test-r8-perf.zip` (10073408 байт).
ZIP SHA256: `f2ad0f751848ee1875754dc834d1a51b2cecdf68cad28930f89a1ee4367e68dd`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `100444559446549d87b6620e81a353e066617ac093ca7a6dbd3e86deaefed574` | `f4cc5cc785f896ff2873166b861bbd67cfee51c8` |
| matrixgame-e610-interactive.nro | `c8c15251038fefdea858b597798a715c7b626e1b85b8314553cda3dfcd5248cb` | `213c20c9d90a1a8868f4573d12b8586809c00bb6` |

Итог: **SOFTWARE PASS / R8 HARDWARE PENDING**. Оба devkitA64 compile/link,
NRO structure/build-ID/title/resources checks — PASS, strong undefined=0.
Каждый map содержит 23 deko3dd object members и 0 deko3d; debug error path
проверен nm. Сверены 695 собственных source SHA, generated original совпал
с r7/r6/r5/r4, сохранён прежний набор двух generated original changes vs E601.
192 защищённых прежних файлов/пакетов не изменены. Игровые ресурсы/frozen/
основная NRO/upstream защищены existing SDK gate. Полный CPU/host replay
и package evidence: [JSON](matrixgame-e610-perf-r8-evidence.json).

Копирование: только две NRO из ZIP в `/switch/matrixgame-e610-r8-test/`.
В hbmenu: MatrixGame r8 Quick Smoke → MatrixGame r8 Interactive. DATA и
три MatrixGame файла остаются на прежних местах; заново копировать не нужно.
После запуска собрать два новых run-каталога и наблюдения об изображении,
скорости и музыке. Interactive завершается Minus+Plus, до 120 секунд после Init.
В run.jsonl сравнивать дельты block_pool_hits/misses, profile.* и время Draw.
Не путать показанные transition frames с четырьмя зачтёнными map frames.
Следующие изменения планируются по новому аппаратному профилю.
