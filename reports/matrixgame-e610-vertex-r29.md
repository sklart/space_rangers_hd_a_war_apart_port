# MatrixGame E610 r29 — dispatch vertex-регистров

После r28 NativeRenderState::Render setter по-прежнему вызывал out-of-line
FixedVertexState::Render для любого ID, кроме texture factor. Для большинства
из 103 render words этот вызов только перебирал проверки ID и возвращал false.
RenderLookup уже содержит constexpr vertex flag, используемый getter.
R29 проверяет этот flag до вызова: только девять vertex ID вызывают прежнюю
функцию. Эти девять веток возвращают true или выбрасывают исходное исключение;
все остальные ID возвращали false без побочных эффектов и FP-арифметики.
Word(id)/LogicalValue остаются перед dispatch, cull/shade/raster и registers.at
идут в прежнем порядке. Для unknown ID Word отклоняет вызов до индекса таблицы.

Изменена одна строка production CPP. Новых таблиц, mutable state, allocations,
ABI, кэша значений или изменений ownership нет. FixedVertexState::Render
и все проверки значений остаются настоящими и неизменными. Удалены только
нецелевые вызовы без побочных эффектов. Все обязательные операции сохранены.
Reference: r28 source 7ca1c4db4d896bfb6bcc2ba348ff34fe54708a26.

## Проверка и границы

Используется неизменённый r28 corpus: 66311 cases, в том числе 25511 scalar
cases/21472 setters и 40800 sampler operations. Проверяются все 103 registers,
четыре rounding modes и FP flags, bounds/NaN/Inf, full state после success/failure,
erased map keys, приоритет unknown ID и повреждённые sampler values.
Сравниваются frozen/current полные stdout на каждой платформе.

Benchmark: прежний scalar workload — 1687552 настоящих Render setters
(16384×103 initial words), 100000 настоящих PrepareDraw controls,
равный 10000-call draw warmup. Production frozen/current CPP at O2/UNDEBUG,
no sanitizers/LTO/fast-math, ABBAAB без параллельных CPU jobs.
Ускорение относится к CPU setter throughput; вклад в Draw/Init/FPS неизвестен.
Коэффициенты предыдущих benchmark не перемножаются.

Большие Windows-бои и полные regression suites не повторяются.
Ресурсы/saves/upstream/frozen/основная NRO и предыдущие пакеты сохраняются.
HARDWARE PENDING: physical GPU/HID/audio/Init/map frames/FPS.
Standalone entry не охватывает unified Campaign/Gov, natural victory/Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-vertex-r29-readme.ru.md).


## Semantic gates

Host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 /1 — PASS.
66311 cases / 49296 expected rejections. Неизменённый corpus r28; полный stdout
frozen/current совпал попарно. Digest host 8810794293108803072,
ARM64 2600753383352677024, Windows 8008852560463349944.
Межплатформенные FP flags/messages не являются acceptance. Assertions active,
ARM64 UBSan/vptr сохранён с прежним type_info symbol workaround.

На каждом проходе всех 103 initial words прежний setter делал 102 вызова
FixedVertexState::Render, новый делает 9. Остальные 93 вызова возвращали false
без побочных эффектов. Число самих setters и value checks не сокращается.
Реальная частота этих операций в кадре Switch пока неизвестна.

Первый вариант с unconditional vertex branch отклонён: setter median
26760 → 24527 мкс, но PrepareDraw control 57450 → 61996 (+7,91%).
Source/причина/измерения сохранены в e610-vertex-r29-checks/rejected-unconditional
и benchmark/1. В принятой для semantic gates форме сохранён исходный bool
результат: `RenderLookup[id].vertex && vertex.Render(id,value)`.
Exploratory /2: setter 28190 → 24240 (−14,01%), PrepareDraw
59398 → 61319 (+3,23%, большой разброс, включая frozen 66195/candidate 56232).
Итоговый замер /3 после semantic gates приведён ниже.


## Итоговый CPU timing

ABBAAB /3, checksum 17297823309272158979 одинаков во всех runs:

| CPU metric, мкс | frozen r28 runs | r29 runs | median до → после |
| --- | --- | --- | --- |
| Setter | 30060 / 27173 / 26589 | 26712 / 26027 / 23890 | 27173 → 26027 (−4,22%) |
| PrepareDraw control | 58717 / 57564 / 57890 | 56708 / 57447 / 60976 | 57890 → 57447 (−0,77%) |

Выгода setter повторилась в двух замерах принятой формы (/2 ~14%, /3 ~4,2%),
но разброс большой и диапазоны пересекаются. Для результата используется /3;
дополнительные timing runs для выбора более выгодной цифры не выполнялись.
Из итогового контроля нельзя заключать об ускорении PrepareDraw: его production
код неизменён, небольшая разница рассматривается как шум. В rejected форме
контроль ухудшился на 7,9%, этот вариант не поставляется.

## Воспроизведение

CPU: `python tools/matrixgame_vertex_dispatch_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_vertex_dispatch_checks.py benchmark` без parallel jobs.
Runner сохраняет frozen r28 CPP, команды/stdout/source hashes в новый numbered
каталог; semantic tests и benchmark exact r28, assertions не ослаблены.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-vertex-r29`. Новому выводу всегда дать новое имя.
Локальный delivery gate: build/matrixgame/e610-vertex-r29-checks/verify_delivery.py
с source commit как аргументом.


## SDK/package и dispatch proof

Source commit: `8ca57b1fc2909d7cc0c415f91d3aea9c31256ece`.
Build ID: `8ca57b1fc290-bc09c38e75b14e38059d`.
Обе отдельные стандартные devkitA64/libnx/deko3d compile/link — PASS,
strong undefined=0. 321 configured TUs: O2/UNDEBUG, exceptions, no fast-math.
Настоящие 23 debug deko3dd object members / 0 release, mixed=false.
Prebuilt Rangers archive unchanged, не выдаётся за новую optimized сборку.
NRO structure/assets, встроенные fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r28 (три исторических delta vs E601).
720 own source hashes проверены. Production diff vs r28 — одна строка,
дополнительно r29 titles/CPU runner; прочие build inputs unchanged.
339 прежних защищённых артефактов SHA unchanged; game/frozen/main NRO gate PASS.

Actual RenderLookup table 630 bytes identical r28 в обеих ELF. Gate извлёк
VertexRegisters и ID-проверки original FixedVertexState::Render из frozen
исходников: девять ID совпали. Vertex byte всех 210 rows соответствует списку.
Actual ARM64 setter после Lookup/LogicalValue читает table + 3*validated ID,
byte offset +2; tbnz bit0 направляет исполнение к настоящему vertex.Render.
После вызова tbz проверяет исходный bool результат и при false переходит к
прежним cull/shade/raster branches. Неvertex ID пропускают этот вызов.
Disassembly и guard block сохранены в evidence. Матрицы r25 machine bytes,
LogicalRules 103 bytes и SamplerLookup r28 28 bytes unchanged.

ZIP: `dist/matrixgame-e610-switch-test-r29-vertex.zip` (10160660 bytes).
SHA256: `ee2f175b98878cabe0b5d750244bd0433055943745382c2eb2d6b898eef670bf`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Игровых ресурсов и сохранений нет. ZIP и распакованный пакет повторно
проверены по SHA; manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `2b1a54b5f4dbd97ac49d5717da9de86761e771d85968825b2db4dc9e1a6d3389` | `137a5265f0652e2e0c894a61681b5cbd44dedd18` |
| matrixgame-e610-interactive.nro | `bb156609d21f9b492ddedec5e771070a04b34aec0393c53e3f0366084636ddec` | `58524d2d779913b48fec373a593b14766a0e9db4` |

SDK output: build/matrixgame/e610-vertex-r29/1. Package command:
`python tools/matrixgame_e610_freeze.py package --build build/matrixgame/e610-vertex-r29/1
--source-commit 8ca57b1fc2909d7cc0c415f91d3aea9c31256ece
--output dist/matrixgame-e610-switch-test-r29-vertex.zip
--readme docs/matrixgame-e610-vertex-r29-readme.ru.md
--rangers-root sdmc:/switch/space-rangers-hd-a-war-apart/game`.
Новому выводу всегда дать новое имя. [Полный evidence](matrixgame-e610-vertex-r29-evidence.json).
Evidence SHA256: `166d3c622703c7158748bc6f23374cc840dcdc8b2a0821735825ef16dedabbbd`.

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → четыре настоящих map frames; затем Interactive → доступные курсор/
кнопки/меню/камера/звук, штатный выход Minus+Plus. Полная инструкция в ZIP,
логи отдельно в /switch/matrixgame-diag/logs/<run-id>.
Повторно копировать ресурсы не требуется. Нового аппаратного исполнения нет;
последние реальные timing относятся к r9, вклад r29 в Draw/Init/FPS неизвестен.
Physical GPU/HID/audio и полнота Init/map frames ещё не приняты.
Unified Campaign/Gov, естественная победа и возврат Galaxy standalone entry не охвачены.
