# MatrixGame E610 r23 — validation-local binding predicate

После r22 ValidateIndexed выполняет buffer-null/Locked/stride guards для
каждого индекса. В этом callback-free read-only validation scope buffer,
lock и stride неизменны. VertexCoverage теперь сохраняет только boolean
invalid_binding_ в constructor; Require по-прежнему проверяет predicate и
negative effective index после declared-index range guard каждого элемента.
Ошибка не бросается в constructor. Приоритет первой ошибки сохранён.

Scope: только private local class в staging_draw.hpp. Нового persistent state,
кеша между draws, изменения Resource/Context API/layout или GPU state нет.
Каждый следующий draw/Discard создаёт новый predicate. Constructor делает
только pure noexcept reads. Context удерживает resource на время validation.
Initialized fallback не вызывает callbacks и не меняет буфер. Поддерживаемый
контракт — один владеющий engine/render thread, как и прежде.

Reference: полный r22 source 14d8a8186d943bb52ae49011a42a5510c58950a8.
CPU runner использует настоящий frozen/current staging header с wrapper в
отдельном каталоге; NativeDrawIndexed body, native state, fixed vertex,
buffer resources неизменны. Full output bytes/errors сравниваются попарно.

CPU tests содержат r22 finite/FP/state tests, плюс full/partial initialized,
Discard→следующий draw, holes, missing/locked VB/IB, negative base, extreme
strides/offsets и диапазоны. Отдельные topology probes для U16/U32, 0..7,
нулевого/ненулевого primitive count, start/minimum до UINT32_MAX, bases -1/0/
INT_MAX, declared range и index values до UINT32_MAX (U16 truncation настоящая).
Проверяются первые ошибки и неизменные buffer versions/lock/cleanup.

Отклонённый кандидат: U16/U32 dispatch вне validation loop, median gather
285469 → 281207 мкс (~1.5%) при перекрытии sample ranges. В production не
включён. Источник/tool/test/решение и результаты сохранены отдельно:
build/matrixgame/e610-validation-r23-checks/rejected и benchmark/1.
Production header восстановлен до r22, затем применён только predicate diff.

Guard candidate benchmark/1: gather median 275216 → 267277 мкс (−2.88%);
все candidate 266548/270489/267277 меньше frozen 276281/275216/272781.
Direct decode control неизменён: 88534 → 87561 мкс (вариация ~1.1%).
Итоговая повторная серия после CPU gates приведена ниже. Это synthetic host
CPU gather U16, не GPU/whole frame/Init/FPS/бой. U32 проходит correctness,
отдельное ускорение U32 не заявляется. Коэффициенты r10–r23 не перемножаются.

Ресурсы/saves/upstream/frozen/основная NRO и прежние пакеты сохраняются.
Большие боевые сценарии и полные regression suites не повторяются.
HARDWARE PENDING: Init/transition/map frames, physical GPU/HID/audio/FPS.
Standalone entry не охватывает единую Campaign/Gov, natural victory и Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-guard-r23-readme.ru.md).

## Итоговые CPU результаты

Host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 /1 — PASS.
23905 cases, 12983 expected rejections, 7776 topology cases, 1948 scenarios,
full_scopes=2, partial_scopes=2. Полный stdout frozen/current равен попарно.
Final digest host 4637017888770985336, ARM64 9782670429877900952,
Windows 18147103199290582331. Межплатформенная разница FP flags/messages
не является acceptance; сравнение локальное. Assertions on, ARM64 UBSan/vptr
сохранён (type_info version workaround как прежде). Cleanup bytes=0;
ошибки draw не публикуют результат, очередность и versions/locks сохранены.

Benchmark/2 ABBAAB: frozen gather 278602/281828/321909 мкс,
candidate 268541/271439/294565; median 281828 → 271439 мкс (−3.69%).
Direct decoder control code unchanged: frozen 93126/89274/90582,
candidate 89588/86713/96316; median 90582 → 89588 мкс (~1.1% variation).
Checksum 5258787986328867328 во всех шести runs совпал. Последние samples
обеих версий медленнее остальных; ranges в /2 перекрываются. Изменение median
повторилось в /1 и /2, выигрыш небольшой и не превращается в оценку FPS.
Первый /1 был полностью разделён по ranges. Дальнейшие timing runs для
подбора более выгодной цифры не выполнялись.

## SDK, package и контрольные суммы

Source commit: `0d31db18d16e34a68d219c8a7ed26d3d8e268d70`.
Build ID: `0d31db18d16e-49f7e44c9fbce8b17a79`.
Обе devkitA64/libnx/deko3d compile/link — PASS, strong undefined=0.
321 configured translation units: O2/UNDEBUG, exceptions, no fast-math;
prebuilt Rangers archive unchanged и не выдаётся за новую optimized сборку.
23 actual debug deko3dd object members / 0 release deko3d, mixed=false.
NRO structure/assets, embedded fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r22 (3 historical delta vs E601).
713 own source hashes проверены. Поведенческий diff — только local predicate
в staging header; дополнительно r23 NACP titles и CPU runner. Остальные r22
build inputs unchanged. 297 прежних защищённых артефактов SHA unchanged;
main NRO/game/frozen SDK checks прошли.

Actual ARM64 ValidateIndexed: два direct Locked loads (IB/VB) сохранены;
в r22 один из них внутри index loop, в обеих r23 ELF внутри цикла — ноль.
Сохранены Initialized/Storage calls и вызов ValidateIndexed из NativeDrawIndexed,
DecodeTo, Read/Storage hoist. Partial Initialized fallback по-прежнему может
читать Locked внутри Resource::Initialized; его реализация не изменена.
Full disassembly, loop range и lock addresses — в evidence. Устранена только
повторная explicit binding guard проверка, обязательные операции не пропущены.

ZIP: `dist/matrixgame-e610-switch-test-r23-guard.zip` (10162710 bytes).
SHA256: `5fcb56e82ea8a4db4e272b8438c09029454d0780a3ab0dee25f8693114dafa9c`.
Ровно 6 файлов: 2 NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Ресурсов/сохранений нет. Manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `5d9be71327ed8e807813eb7fe23573dd6d99cf5ad4a26efcffd3e5cca9f2c0ca` | `703566a719bb00345f9be88e4b88e1daec310f19` |
| matrixgame-e610-interactive.nro | `532fff3cb7729a743aacd5b74844b39239b5cde07d1519d646949c4ff6f193f3` | `75ccb345a030d805239ca2fc557d4f6597acbdc3` |

## Воспроизведение и границы

CPU: `python tools/matrixgame_validation_guard_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_validation_guard_checks.py benchmark` без parallel jobs.
Runner сохраняет frozen r22 CPP/header, actual gather, wrapper, hashes и commands
в новый numbered каталог; тестовая сборка использует versioned header по variant.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-guard-r23` — создаётся новый numbered каталог.
Package: `python tools/matrixgame_e610_freeze.py package --build
build/matrixgame/e610-guard-r23/1 --source-commit 0d31db18d16e34a68d219c8a7ed26d3d8e268d70
--output dist/matrixgame-e610-switch-test-r23-guard.zip --readme
docs/matrixgame-e610-guard-r23-readme.ru.md --rangers-root
sdmc:/switch/space-rangers-hd-a-war-apart/game` (для нового вывода новое имя).
Локальный gate: build/matrixgame/e610-guard-r23-checks/verify_delivery.py с source
commit как аргументом. [Полный evidence](matrixgame-e610-guard-r23-evidence.json).

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → 4 настоящих map frames, затем Interactive → доступные кнопки/меню/
камера/звук и Minus+Plus. Полная инструкция в ZIP; логи отдельно в
/switch/matrixgame-diag/logs/<run-id>. Ресурсы повторно копировать не требуется.
Физические GPU/HID/audio, Init/Draw/FPS не приняты. Единая Campaign/Gov,
естественная победа и Galaxy return этим standalone entry не охватываются.

Evidence SHA256: `b2e855fd679b9ba8eebc6d89830828c52ab02581dff7cf2ee0439b18d467b200`.
