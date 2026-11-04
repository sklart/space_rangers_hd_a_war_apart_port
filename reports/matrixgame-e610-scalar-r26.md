# MatrixGame E610 r26 — compile-time scalar setter rules

В LogicalValue каждый RenderState setter сравнивал неизменное имя регистра
с несколькими списками строк. В r26 constexpr LogicalRules хранит для каждого
из 103 NativeRenderWords uint8 bitmask: Bool/Float/Fill/Fog/Stencil/Mask/
BlendFactor/BlendOp. Все группы получены теми же Is/starts_with условиями при
компиляции. Это не кэш значений и не пропуск обязательной validation.

LogicalValue принимает flags, но value проверяет каждый раз теми же integer
сравнениями и std::isfinite(bit_cast<float>) в прежнем порядке. Используются
независимые mask bits, поэтому даже возможное пересечение правил не теряется.
Все exception messages и категории прежние. Lookup(id) идёт первым, как раньше
через Word(id); затем logical validation, vertex/raster/combiner и registers.at.
Word private helper, LogicalValue signature и Render caller сохранены; flags
получаются по index metadata reference в исходном NativeRenderWords array.
Случай missing map key по-прежнему может изменить component до map::at failure,
если original так делал; новый тест проверяет и это наблюдаемое состояние.

Scope: только private rule classification/Render setter в native_device_state.cpp.
Getters, PrepareDraw, NativeStateStore/leases/COW, API/layout, allocation/cleanup,
shaders, uniforms, native GPU operations, raster validation и ресурсы не меняются.
Reference — полный r25 source d6ad833e2a4bb9418abfe5480065a1ce7717be64.
Все r25 matrix/UV и r23 binding изменения сохранены unchanged.

CPU probe сохраняет прежние 4039 state lookup tests, расширяет их всеми 103
registers, numeric boundaries, +/-zero/min subnormal/min normal/max finite/
Inf/quiet/signaling NaN, 4 rounding modes и erased target key. После успеха и
исключения хешируется raw logical map, все getters (с их errors), raster,
combiner/bindings, samplers и полные vertex uniforms. FP flags фиксируются
сразу после setter до snapshot. Unknown IDs с injected map entry должны
оставаться unknown и не менять fake value. Исходные tests и assertions сохранены.

Exploratory benchmark/1: setter median 74161 → 38077 мкс, но unchanged
PrepareDraw control 60851 → 73419 мкс с большим first-candidate outlier 128386.
Источник bench/tool и timing note сохранены в checks/exploratory; /1 не выдан
за окончательный speedup. Добавлен одинаковый 10000-call PrepareDraw warmup
перед измерением обеих версий, чтобы прогреть state/code и сравнить steady-state.
Первое измерение сохраняется целиком. Дальнейшие результаты приведены ниже.
Benchmark: 1687552 actual Render setters (16384×103 initial values), затем
100000 actual PrepareDraw controls. O2/UNDEBUG, no sanitizers/LTO/fast-math,
без parallel jobs. Renderer/GPU не имитируются, они не входят в CPU workload.

Это CPU setter stage, не whole Draw/Init/FPS/бой. Частота setters/вклад в кадр
на Switch неизвестны. Коэффициенты r10–r26 не перемножаются.
Ресурсы/saves/upstream/frozen/основная NRO и прежние пакеты сохраняются.
Большие боевые сценарии и полные regression suites не повторяются.
HARDWARE PENDING: Init/transition/map frames, physical GPU/HID/audio/FPS.
Standalone entry не охватывает единую Campaign/Gov, natural victory и Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-scalar-r26-readme.ru.md).

## Выбор более узкой формы

Вариант с прямым Lookup→LogicalRules и удалённым Word helper не принят: warm
benchmark/2 setter 72283 → 28397 мкс, но PrepareDraw control 56070 → 63125
(~12.6% хуже). Даже без изменения C++ PrepareDraw body такая регрессия control
не была проигнорирована. Источник/решение сохранены в rejected-direct-lookup.
Принятый кандидат сохраняет Word helper, LogicalValue(const NativeStateWord&,v)
и исходную первую строку Render. Меняется только классификация внутри LogicalValue.
Benchmark/3: setters 73608 → 29255; PrepareDraw control 58644 → 57288 мкс,
с перекрывающимися ranges/отдельными outliers. Итоговый замер после повторных
semantic gates приведён ниже.

## Итоговые CPU gates

Semantic /2: host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 — PASS.
25511 cases, 19120 expected rejections, setter_cases=21472, all103/rounding4.
Полный stdout frozen/current одинаков попарно. Digest host 18307926985438231120,
ARM64 11135136558695693040, Windows 16530679332089519352. Межплатформенная
разница flags/messages не является acceptance. Assertions active; ARM64
workaround скрывает только type_info symbol, UBSan/vptr сохранён.

Итоговый benchmark/4 ABBAAB (одинаковый warmup):
- Setter frozen 73934/73665/72602 мкс; candidate 29721/29778/29430;
  median 73665 → 29721 мкс (−59.65%, 2.48×) / 1687552 operations.
- Unchanged PrepareDraw control frozen 56886/58172/56646;
  candidate 57398/57362/57189; median 56886 → 57362 мкс (+0.84%).
  Ranges перекрываются; устойчивой +12.6% regression первого варианта нет.
- Checksum 17297823309272158979 одинаков во всех шести runs.

Выигрыш setter повторился в /3 и /4; control в пределах ~1% в финальном run.
Это локальный CPU setter throughput, не ускорение игры/FPS. После этого
дополнительные timing runs для выбора лучшей цифры не делались.

## SDK/package и compiled-rule proof

Source commit: `73c86b5c6ccbd4b2912a34f8107faf1b9fd7cd87`.
Build ID: `73c86b5c6ccb-377f722407274dc80caf`.
Обе стандартные devkitA64/libnx/deko3d compile/link — PASS, strong undefined=0.
321 configured translation units: O2/UNDEBUG, exceptions, no fast-math;
prebuilt Rangers archive unchanged, не выдаётся за новую optimized сборку.
23 actual debug deko3dd object members / 0 release deko3d, mixed=false.
NRO structure/assets, embedded fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r25 (3 historical delta vs E601).
717 own source hashes проверены. Изменён только scalar LogicalValue участок;
весь CPP prefix до него и suffix от constructor до EOF совпадают с r25.
API/layout, Render caller/getters, PrepareDraw, leases/COW unchanged.
318 прежних защищённых артефактов SHA unchanged; main NRO/game/frozen gate PASS.

В каждой actual ELF LogicalRules — readonly table из 103 bytes. Gate извлёк
группы условий из frozen LogicalValue и serialized names из frozen
NativeRenderWords, вычислил expected flags и побайтно сравнил все 103 rows с
compiled table. Всё совпало. GCC встроил новый classifier в реальный setter:
проверены точные ADRP/ADD table address и indexed byte load; string classification
calls отсутствуют. Value bounds/nonfinite, vertex/raster/combiner и map failure
пути остались настоящими. Matrix Multiply bytes совпадают с tested r25 kernel
SHA a575632d8915cd0fd1bc6df10d9c3564789a270a1baad17eae38bd3bd0be5195.
Это доказательство включения кода, не hardware runtime или FPS.

Проверочный parser сначала ошибочно сканировал все 116 metadata rows header,
включая sampler; исправлен scope на точный NativeRenderWords array из 103.
Первый assembly gate ожидал отдельный LogicalValue symbol, но actual GCC
inlines его в setter; проверка уточнена на реальные инструкции/table load.
Предположения и failures сохранены в checks/failed-parser-assumption.json.
Production source/NRO при уточнении gate не менялись.

ZIP: `dist/matrixgame-e610-switch-test-r26-scalar.zip` (10161078 bytes).
SHA256: `f8dd7d31304b277cffde06208278da537cd5fd3eab7d1df5f1d90719b1ef530e`.
Ровно 6 файлов: 2 NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Ресурсов/сохранений нет. Manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `035bc173796650a0c673f444265bb0f61361f7b9ed80563dc90af7da5b3d6017` | `92fac5397cfa9c068f458c453c8d3eee41c3eaeb` |
| matrixgame-e610-interactive.nro | `7cc79b4f1b313ed2b1f55173d4cac056e3bcd691d18b9ca265f7e2f052a5249c` | `319c2ee9a92ea2fc632664a0f0630edaca66494a` |

## Воспроизведение и аппаратные границы

CPU: `python tools/matrixgame_scalar_rules_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_scalar_rules_checks.py benchmark` без parallel jobs.
Runner сохраняет frozen r25 CPP, commands/stdout/source hashes и результаты
в новый numbered каталог. Probe имеет одинаковый state/draw warmup.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-scalar-r26` — создаётся новый numbered каталог.
Package: `python tools/matrixgame_e610_freeze.py package --build
build/matrixgame/e610-scalar-r26/1 --source-commit 73c86b5c6ccbd4b2912a34f8107faf1b9fd7cd87
--output dist/matrixgame-e610-switch-test-r26-scalar.zip --readme
docs/matrixgame-e610-scalar-r26-readme.ru.md --rangers-root
sdmc:/switch/space-rangers-hd-a-war-apart/game` (новому выводу дать новое имя).
Локальный gate: build/matrixgame/e610-scalar-r26-checks/verify_delivery.py с source
commit как аргументом. [Полный evidence](matrixgame-e610-scalar-r26-evidence.json).

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → 4 настоящих map frames, затем Interactive → доступные кнопки/меню/
камера/звук и Minus+Plus. Полная инструкция в ZIP; логи отдельно в
/switch/matrixgame-diag/logs/<run-id>. Ресурсы повторно копировать не требуется.
Вклад setter в реальный кадр неизвестен; FPS/Init не измерены.
Физические GPU/HID/audio не приняты. Единая Campaign/Gov, естественная победа
и возврат Galaxy этим standalone entry не охватываются.

Evidence SHA256: `d3503f9bef2afdaf3d9d4786e595ed2b07cba2daa0cc0a4a826ca5a2358930b5`.
