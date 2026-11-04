# MatrixGame E610 r34 — поиск initialized coverage в фрагментированном буфере

Resource::Initialized линейно просматривал initialized_ при каждом запросе.
Это реальная проверка доступных байтов vertex/index buffers, в том числе
fallback для частично заполненных буферов в indexed validation. R34 сохраняет
прямую проверку первого интервала и использует lower_bound по концам остальных.
Unmap уже сортирует и объединяет пересекающиеся/соседние ranges, поэтому их
концы строго возрастают. Если найден первый last>=offset+bytes, остаётся
проверить first<=offset. Это один покрывающий интервал, а не разрешение читать
через gap. Если интервала нет, результат false, как раньше.

Исходные lock/bounds и zero-byte guards идут в прежнем порядке; end вычисляется
после защиты от overflow. Полностью заполненный/первый интервал остаётся
прямым fast path. Новый поиск не мутирует version, initialized metadata,
storage, bindings, refs или бюджет. Нет allocations, новых fields/API, кеша
между запросами или изменений GPU ownership. Map/Unmap/Discard/Storage unchanged.
Reference: r33 source c8f793daf5921619785064fe30e391c94565757c.

## Отклонённый packet writer

Исследована очистка только gaps/tail вместо memset 4096 перед payload copy.
Сохранён alias/null-vertex fallback, но cached host write median 46488 → 61897
мкс (+33,15%). Снижение байтов записи само по себе не доказывает ускорение.
Switch uncached memory не измерялась: NOT SELECTED / NOT SHIPPED.
Source/header/runner/benchmark/probe и результат сохранены в
build/matrixgame/e610-packet-r34-checks/not-selected и benchmark/1.
Semantic gates для этого эксперимента не запускались; packet header exact r33.

## Проверка

Frozen — полный Git archive port/matrixgame r33 с оригинальными sources/headers.
Сохранён предыдущий corpus, включая ctor allocation-failure cleanup.
Добавлена независимая per-byte bitmap + prefix-count модель coverage.
После каждой записи проверяются все offset/length 0..128, overflow границы,
zero lengths, version/lock. Отдельные проверки при mapped state обязаны вернуть
false даже для zero-byte query. Real Context/CreateVertex/CreateIndex/Map/Unmap,
Normal/NoOverwrite/Discard, 64 gaps, touching/overlap merge, full update и
повторный discard. После Release/ClearBindings бюджет снова нулевой.

Всего 266 дополнительных state cases и 4434986 query checks; это targeted CPU
property test, не Windows battle/полная regression suite. Read result сравнивается
с моделью, не повторяющей range search. Никакие mandatory operations не заменены.

Benchmark: 2097152 queries в fully initialized buffer и столько же в буфере
с 256 раздельными 16-byte ranges. Запросы фиксированной длины 8 обходят все
смещения (включая invalid tail), checksum одинаков. 10000-query warmup.
Контрольные r33 constructor/setter/getter/PrepareDraw/StencilProbe сохранены.
O2/UNDEBUG, no sanitizer/LTO/fast-math; ABBAAB без parallel CPU jobs.

Exploratory /2: fragmented median 121292 → 34316 мкс (3,53×), full-range
2940 → 2613 (−11,12%); прочие controls в пределах 5% либо быстрее.
Checksum 2654920981528516123. Первое /1 не скомпилировалось из-за отсутствия
cstring в новом benchmark, до исполнения; добавлен include, production code
не менялся. Исходник ошибочного test TU и failure log сохранены.

Частота многодиапазонных buffers в настоящем Switch кадре неизвестна;
коэффициент не переносится на whole Draw/Init/FPS и не перемножается с прежними.
Ресурсы/saves/upstream/frozen/основная NRO и предыдущие пакеты сохраняются.
HARDWARE PENDING: physical GPU/HID/audio/Init/map frames/FPS.
Standalone entry не охватывает unified Campaign/Gov, natural victory/Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-ranges-r34-readme.ru.md).

## Итоговый CPU замер и семантика

Итоговый ABBAAB /3 после semantic gates (микросекунды, median трёх запусков):

| Проверка | Frozen r33 | R34 |
| --- | ---: | ---: |
| 256 фрагментов | 120069 | 34675 |
| Полный буфер | 2860 | 2595 |
| Setter | 23655 | 23638 |
| Mixed getter | 38627 | 39342 |
| Raster getter | 7963 | 8112 |
| PrepareDraw | 57213 | 57949 |
| StencilProbe | 13511 | 13075 |
| Constructor/destructor | 89948 | 92280 |

Целевой поиск: 3,46× / −71,12%; full buffer −9,27%. Controls в пределах 5%
либо быстрее. Все 12 измерений /2+/3: fragmented 120680,5 → 34546,5 мкс
(3,49× / −71,37%); полный буфер 2900 → 2604 (−10,21%). У всех 12 запусков
одинаковый checksum 2654920981528516123; агрегированные controls также в
пределах 5% либо быстрее. Это стоимость поиска, не всего кадра.

Host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 /fp:strict — PASS.
Frozen/current полные semantic stdout совпадают попарно. ARM64 linker version
map локализует конфликтующий type_info symbol, UBSan vptr не отключён.
Предыдущий corpus, allocation failure на каждом из 103 GCC/104 MSVC nodes,
cleanup и zero live blocks сохранены. Итоговые cases/rejections/digests:

| Платформа | Cases | Rejections | Digest |
| --- | ---: | ---: | ---: |
| host | 87162 | 50832 | 4061713791257964148 |
| ARM64/QEMU | 87162 | 50832 | 15238033043485349076 |
| Windows | 87163 | 50832 | 1887459644069839471 |

Разница cases на Windows — прежний MSVC map head node allocation case.
Новый oracle даёт 266 state cases и 4434986 queries на каждой платформе.
Frozen port извлечён целиком из Git archive r33; hashes именно compiled bytes
и Git blob записаны отдельно, полные тексты совпадают после CRLF/LF normalization.

Воспроизведение: `python tools/matrixgame_range_search_checks.py host`
(также arm64, windows, benchmark). Каждый запуск создаёт новый numbered output.
Большие Windows battles и полные регрессии не перезапускались.

## SDK и отдельный пакет

Source commit: `da540f87431e338714cd05957405556c1017be3a`.
Build ID: `da540f87431e-6f9d58491138f5319fc8`.
Стандартная сборка: `python tools/matrixgame_current_diagnostic_sdk.py
--output-root build/matrixgame/e610-ranges-r34`. Текущий output — /1.
Обе libnx/deko3d devkitA64 compile/link — PASS; strong undefined=0.
321 configured TUs: O2/UNDEBUG, exceptions, no fast-math. Link maps подтверждают
23 debug deko3dd object members / 0 release, mixed=false. Prebuilt Rangers
archives unchanged, не выдаются за новую optimized сборку.
NRO structure/assets, встроенные fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated originals exact r33; три прежних changes vs E601 сохранены.
725 own source hashes verified. Production delta только body Resource::Initialized,
дополнительно r34 titles/runner. Packet header и остальные build inputs exact r33.
374 защищённых прежних артефакта SHA unchanged; game/frozen/main NRO gate PASS.
Старые пакеты/evidence не перезаписывались.

Actual ARM64 Resource::Initialized обеих ELF: 128 → 200 bytes. Lock/bounds/zero
guards предшествуют вычислению end; первый интервал проверяется прямо.
Tail использует ASR #1 для midpoint, stride16 и last load +8; после поиска
проверяется first<=offset. Leaf: нет BL/allocations/stack frame. Полный disassembly
сохранён в evidence. Дополнительный размер 72 bytes — плата за логарифмический
поиск; частота этого tail в Switch кадре неизвестна.
RenderLookup 630, LogicalRules 103 и SamplerLookup 28 bytes, а также Matrix r25
machine bytes exact r33. Allocation fault hooks, CPU tests/benchmarks отсутствуют
в SDK ELF/compile_commands. Это static/compile/link proof, не аппаратное исполнение.

ZIP: `dist\matrixgame-e610-switch-test-r34-ranges.zip` (10160914 bytes).
SHA256: `b86242b03b07b8fe141f1d5f22e116f49b914aa1213307abbece846726a5ea55`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Игровых ресурсов и сохранений нет. ZIP и распакованные файлы проверены по SHA.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `95584261d2e621faea4e85193cdbb9a2933bb8fa0f5f8016d427445b103d6679` | `954b1c1194af81e456ffc42ff5a57cc067741b21` |
| matrixgame-e610-interactive.nro | `f01cc316c4ca925034ae05f3610e3746e64cc9c9e04c7bcd92ba207dc61d3d59` | `affdea515058bb1472cf1636c25bac1dc40019ee` |

Package command: `python tools/matrixgame_e610_freeze.py package
--build build/matrixgame/e610-ranges-r34/1
--source-commit da540f87431e338714cd05957405556c1017be3a
--output dist/matrixgame-e610-switch-test-r34-ranges.zip
--readme docs/matrixgame-e610-ranges-r34-readme.ru.md
--rangers-root sdmc:/switch/space-rangers-hd-a-war-apart/game`.
При повторении использовать новое имя вывода. Delivery gate:
`python build/matrixgame/e610-ranges-r34-checks/verify_delivery.py
 da540f87431e338714cd05957405556c1017be3a`.
[Полный evidence](matrixgame-e610-ranges-r34-evidence.json).
Evidence SHA256: `74463e18afbec8828c98ae5690a087fc9fb49276455c378e9a8763219f9b7abf`.

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Два ручных запуска: Quick Smoke до четырёх настоящих map frames, затем
Interactive до 120 секунд после Init; штатный выход Minus+Plus.
NRO копировать отдельно в /switch/matrixgame-e610-r34-test/; ресурсы повторно
копировать не требуется. Логи: /switch/matrixgame-diag/logs/<run-id>.
Полные пути ресурсов и ограничения — в README архива.
Последние реальные timings относятся к r9. Physical GPU/HID/audio, полнота Init
и map frames ещё не приняты. Unified Campaign/Gov, естественная победа и возврат
в Galaxy standalone entry не охвачены. Переноса/запуска на Switch не выполнялось.
