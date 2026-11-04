# MatrixGame E610 r27 — reuse raster membership lookup

После r26 Render setter всё ещё линейно искал id в 11 RasterRegisters после
Word/LogicalValue и vertex/cull/shade dispatch. RenderLookup уже содержит
raster index (1-based), построенный constexpr из того же RasterRegisters и
используемый getter. R27 заменяет единственный std::find predicate на
RenderLookup[id].raster. Сам raster.Apply(id,value) и его validation неизменны.

Word(id) вызывает Lookup и отклоняет unknown/out-of-range ID раньше LogicalValue
и нового чтения массива. Поэтому id уже validated, index bound безопасен.
Порядок всех checks и register.at записи прежний; missing map может менять
component перед ошибкой, как раньше. Обязательные операции не пропускаются.
Никаких новых таблиц/кеша/fields/ABI/allocations или changes COW/ownership.
Scope: одна строка в NativeRenderState::Render setter; всё остальное CPP exact r26.
Reference: r26 source 73c86b5c6ccbd4b2912a34f8107faf1b9fd7cd87.

Использован прежний расширенный scalar corpus r26: все103 registers,
bounds/NaN/Inf/signed zero/FP flags в четырёх rounding modes, unknown IDs с
injected map entries, erased target keys, полное наблюдаемое состояние после
success/failure, draw modes/samplers. Tests/assertions не менялись.

Первый ABBAAB /1: setter median 30438 → 28036 мкс (−7.9%) /1687552 operations;
PrepareDraw control 58677 → 59240 (+~1%). Есть outlier в последнем candidate.
Окончательный повторный замер после semantic gates ниже. Benchmark: actual
Render setters (16384×103 initial words), 100000 actual PrepareDraw controls,
одинаковый 10000-call warmup. O2/UNDEBUG, no sanitizers/LTO/fast-math, no parallel jobs.
Это CPU setter throughput, не whole Draw/Init/FPS/бой; вклад на Switch неизвестен.
Коэффициенты r10–r27 не перемножаются. Матрицы r25 и остальные draw paths unchanged.

Ресурсы/saves/upstream/frozen/основная NRO и прежние пакеты сохраняются.
Большие боевые сценарии и полные regression suites не повторяются.
HARDWARE PENDING: Init/transition/map frames, physical GPU/HID/audio/FPS.
Standalone entry не охватывает единую Campaign/Gov, natural victory и Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-raster-r27-readme.ru.md).

## Итоговые CPU результаты

Host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 /1 — PASS. 25511 cases,
19120 expected rejections, setter_cases=21472. Полный stdout frozen/current
одинаков попарно. Digest host 18307926985438231120, ARM64
11135136558695693040, Windows 16530679332089519352. Межплатформенная разница
FP flags/messages не является acceptance. Assertions active, ARM64 UBSan/vptr
сохранён с прежним type_info symbol workaround. Tests не переписывались.

Итоговый ABBAAB /2:
- Setter frozen 28696/28227/28455 мкс, candidate 26730/26420/29115;
  median 28455 → 26730 мкс (−6.06%). Последний candidate — outlier.
- PrepareDraw control frozen 57517/56624/60167, candidate 58234/58334/58274;
  median 57517 → 58274 мкс (+1.32%), ranges перекрываются.
- Checksum 17297823309272158979 одинаков во всех runs.

Setter выигрыш повторился в /1 (~7.9%) и /2 (~6.1%). Изменение небольшое;
общий FPS/Init gain не заявляется. После этого дополнительные timing runs
для выбора более выгодной цифры не выполнялись.
