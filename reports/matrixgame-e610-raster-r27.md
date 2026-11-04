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

## SDK/package и membership proof

Source commit: `b9122aadf9b346f56fc77a019084c3da4510f683`.
Build ID: `b9122aadf9b3-b7060a12f58ee098c784`.
Обе стандартные devkitA64/libnx/deko3d compile/link — PASS, strong undefined=0.
321 configured translation units: O2/UNDEBUG, exceptions, no fast-math;
prebuilt Rangers archive unchanged, не выдаётся за новую optimized сборку.
23 actual debug deko3dd object members / 0 release deko3d, mixed=false.
NRO structure/assets, embedded fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r26 (3 historical delta vs E601).
718 own source hashes проверены. Production diff vs r26 — один predicate;
дополнительно r27 titles и CPU runner. Остальные build inputs unchanged.
325 прежних защищённых артефактов SHA unchanged; game/frozen/main NRO gate PASS.

Actual RenderLookup table 630 bytes identical r26 в обеих ELF. Gate извлёк
RasterRegister enum IDs и ordered RasterRegisters list из frozen header и
сравнил raster byte всех 210 rows с исходным membership/index (11 registers).
Все значения совпали. Scalar LogicalRules table (103 bytes) unchanged;
Matrix Multiply machine bytes unchanged от tested r25 kernel.
Actual ARM64 setter вычисляет table base + 3*validated ID, загружает один byte
по offset +1 и ветвится к настоящему MeshRasterState::Apply. В этом блоке нет
loads из старого списка из 11 words. Lookup/LogicalValue/vertex/cull/shade и
registers.at/exception пути сохранены. Disassembly/table hashes в evidence.

ZIP: `dist/matrixgame-e610-switch-test-r27-raster.zip` (10160857 bytes).
SHA256: `fbc2a20066e8513449b97f3c405263faa8be438bc6bf8a609ae31c3bbf8a05c0`.
Ровно 6 файлов: 2 NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Ресурсов/сохранений нет. Manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `7831ed25378e44f1d2fdf42e2cb8cbfbe177eb67c9799f18d92e163b973931c8` | `6c9b70e93ff5efcfd73b7330cd112f81486d035d` |
| matrixgame-e610-interactive.nro | `16144ef52934090f3f68b1e5134e965b24471b4fbdc48790edbaa91236bf62f7` | `d297a9add141d585e86501b85b658121e7c9a01e` |

## Воспроизведение и аппаратные границы

CPU: `python tools/matrixgame_raster_membership_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_raster_membership_checks.py benchmark` без parallel jobs.
Runner сохраняет frozen r26 CPP, commands/stdout/source hashes и результаты
в новый numbered каталог; test корпус и warmup unchanged от r26.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-raster-r27` — создаётся новый numbered каталог.
Package: `python tools/matrixgame_e610_freeze.py package --build
build/matrixgame/e610-raster-r27/1 --source-commit b9122aadf9b346f56fc77a019084c3da4510f683
--output dist/matrixgame-e610-switch-test-r27-raster.zip --readme
docs/matrixgame-e610-raster-r27-readme.ru.md --rangers-root
sdmc:/switch/space-rangers-hd-a-war-apart/game` (новому выводу дать новое имя).
Локальный gate: build/matrixgame/e610-raster-r27-checks/verify_delivery.py с source
commit как аргументом. [Полный evidence](matrixgame-e610-raster-r27-evidence.json).

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → 4 настоящих map frames, затем Interactive → доступные кнопки/меню/
камера/звук и Minus+Plus. Полная инструкция в ZIP; логи отдельно в
/switch/matrixgame-diag/logs/<run-id>. Ресурсы повторно копировать не требуется.
Вклад setter в реальный кадр неизвестен; FPS/Init не измерены.
Физические GPU/HID/audio не приняты. Единая Campaign/Gov, естественная победа
и возврат Galaxy этим standalone entry не охватываются.

Evidence SHA256: `3a7d45a6805f355ccda9ff002edf3b2abbed6f3fd18aeea451d001011da31ea1`.
