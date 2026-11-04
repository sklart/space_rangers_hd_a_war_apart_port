# MatrixGame E610 r22 — inline finite checks, cold exception construction

После r21 actual ARM64 DecodeTo всё ещё вызывал отдельный Finite(float) на
каждый position/normal/UV float. Тело helper включает exception allocation,
и GCC оставлял функцию вне decode loop. Кандидат сохраняет std::isfinite и
выносит только throw std::invalid_argument("nonfinite fixed vertex value")
в anonymous noreturn NonfiniteVertexValue. GCC cold/noinline и MSVC noinline
не дают компилятору вернуть exception construction в горячий helper.
Finite становится inline; arithmetic/FP flags compiler options не меняются.

API/layout/storage, FVF bounds, r21 memcpy specialization, finite порядок,
ошибки и output/publication unchanged. Нет кеша, fast-math или ослабления
обязательной проверки. Helper используется также Transform/Material/Light,
поэтому адресно проверяются и они. Не включены изменения Render/GPU/ресурсов.

Reference: полный r21 source 3282afcd569f84a633108abefbfbf53e3e4bb1a0,
native_device_state.cpp/fixed_vertex.cpp; actual indexed body одинаков.
Все 864 layouts и 2250 unaligned/FP edge probes из r21 сохранены. Добавлены
2043 clients probes: 11 transforms × 16 matrix elements, 17 material fields,
11 light components, +/-zero/subnormal/max finite/Inf/quiet/signaling NaN,
FP flags, unchanged state after invalid input; три denied operator-new cases
сравнивают тип/сообщение исключения и полное prepared state с frozen.
Библиотечное выделение exception message может различаться между платформами;
сравнение попарное, assertions не ослабляются. Ошибки прерывают до Record.

Exploratory benchmark/1 ABBAAB: direct 199275 → 90370 мкс,
gather 363296 → 277580 мкс. После него test расширен на остальных пользователей
Finite; exploratory runner сохранён. Итоговый замер без parallel jobs ниже.
CPU probe: 10 млн direct vertices, 4096 indexed draws/8388608 decoded vertices,
25165824 indices. Host O2 UNDEBUG, no sanitizers/LTO/fast-math.
Не является timing GPU/whole frame/Init/боя/FPS. Коэффициенты r10–r22 не
перемножаются. Последний hardware Draw r9 не является замером r22.

Ресурсы/saves/upstream/frozen/основная NRO и предыдущие пакеты сохраняются.
Большие боевые сценарии и полные regression suites не повторяются.
HARDWARE PENDING: Init→transition→4 map frames, physical HID/GPU/audio/FPS.
Standalone entry не охватывает единую Campaign/Gov, natural victory и Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-finite-r22-readme.ru.md).

## CPU acceptance

Semantic /1: host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 — PASS.
FINITE clients=2043, float_edges=2250, cases=14165, rejected=5407,
layouts=864, gathers=144, no_publication=216. Full stdout frozen/current равен
на каждой платформе; host digest 15426235832256809374, ARM64
17221440035600571454, Windows 15900208132274097733. Межплатформенная разница
из-за FP flag constants/stdlib messages не является ошибкой; сравнение локальное.
Assertions on; ARM64 workaround скрывает только type_info symbol, UBSan/vptr
сохранён. DecodeTo не выделяет heap; data/output/guard order unchanged.
Historical ownership/Unmap/transition proof не выдаётся за новый hardware test.

## Итоговый ABBAAB /2

Direct frozen: 196263/195669/199330 мкс; candidate 87682/87196/94312.
Median 196263 → 87682 мкс (−55.32%, 2.24×) на 10 млн vertices.
Gather frozen: 358563/358832/369774; candidate 276393/271380/307078.
Median 358832 → 276393 мкс (−22.97%, 1.30×) на 4096 draws.
Checksum 5258787986328867328 одинаков во всех шести runs. Последний candidate
run медленнее остальных; даже он быстрее всех frozen gather runs. Выигрыш
подтверждён обеими сериями, но проценты относятся только к CPU probe.
Новых аппаратных измерений Init/Draw/FPS нет.

## SDK/package и SHA

Source commit: `14d8a8186d943bb52ae49011a42a5510c58950a8`.
Build ID: `14d8a8186d94-bcd7f004a2c56733abef`.
Обе стандартные devkitA64/libnx/deko3d compile/link — PASS. Strong undefined=0.
321 configured translation units: O2/UNDEBUG, exceptions, no fast-math;
prebuilt Rangers archive unchanged, не выдаётся за новую optimized сборку.
23 debug deko3dd object members / 0 release deko3d, mixed=false.
NRO segments/assets, embedded fonts/shaders, NACP titles, ELF→NRO build IDs
проверены. Generated original files identical r21 (3 historical delta vs E601).
712 own source hashes проверены. Production diff vs r21 — только Finite helper;
дополнительно NACP r22 titles и CPU runner. Остальные build inputs unchanged.
290 прежних защищённых артефактов SHA unchanged; game/frozen/main NRO SDK gate PASS.

Actual ARM64 DecodeTo: обычные вызовы Finite удалены. Сохранена точная r21
unsigned bit classification: ((float_bits << 1) mod 2^32) >= 0xff000000 → ошибка.
5 статических мест: position loop, UV loop, три развёрнутых normal checks.
Все 5 веток ведут в NonfiniteVertexValue с настоящими invalid_argument и
__cxa_throw. NativeDrawIndexed по-прежнему вызывает DecodeTo; r21 fixed memcpy
copies и r19 Read/Storage hoist сохранены. Полный disassembly обеих ELF,
old Finite и classifier sites — в evidence.

Первый assembly gate ошибочно ожидал fcmp, затем 3 sites вместо 5 (normal loop
развёрнут). После просмотра actual r21/r22 assembly gate уточнён на реальную
bit policy и все 5 branches. Production source и ELF/NRO не менялись.
Ошибочные assumptions сохранены в checks/failed-assembly-assumption.json;
это исправление проверочного скрипта, а не скрытие runtime failure.

ZIP: `dist/matrixgame-e610-switch-test-r22-finite.zip` (10162598 bytes).
SHA256: `39d993a497e061688c1263f2f61f817f00a379040718855b3ac104ffe8c5b264`.
6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Ресурсов/сохранений нет. Manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `6ef01961e1b894f6450f951ba32dae846588d1e0ac389b6f651b7ed32826c468` | `24c07328152fa6fa5ceceff167cb860b2862d242` |
| matrixgame-e610-interactive.nro | `127b5bbce95869de507c4deebbac7e573ec6866a25b5985c7e0f596479f4d8bf` | `8e5439e86adef927370bd93cf31f85ecb0e9af27` |

## Воспроизведение и аппаратные границы

CPU: `python tools/matrixgame_finite_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_finite_checks.py benchmark` без parallel jobs.
Runner сохраняет frozen r21 CPP, actual indexed body, command logs, stdout,
source/input hashes и результаты в новый numbered checks каталог.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-finite-r22` — создаётся новый numbered каталог.
Package: `python tools/matrixgame_e610_freeze.py package --build
build/matrixgame/e610-finite-r22/1 --source-commit 14d8a8186d943bb52ae49011a42a5510c58950a8
--output dist/matrixgame-e610-switch-test-r22-finite.zip --readme
docs/matrixgame-e610-finite-r22-readme.ru.md --rangers-root
sdmc:/switch/space-rangers-hd-a-war-apart/game` (новому выводу дать новое имя).
Локальный gate: build/matrixgame/e610-finite-r22-checks/verify_delivery.py с source
commit как аргументом. [Полный evidence](matrixgame-e610-finite-r22-evidence.json).

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Два ручных запуска: Quick → 4 настоящих map frames, Interactive → доступные
кнопки/меню/камера/звук и Minus+Plus. Полная инструкция включена в ZIP.
Логи — /switch/matrixgame-diag/logs/<run-id>.
Физические GPU/HID/audio, Init/Draw/FPS не приняты. Единая Campaign/Gov,
естественная победа и возврат Galaxy этим standalone entry не охватываются.

Evidence SHA256: `7abbdd0790d24cccb503163b40db13fa9acee7f8ebad1db3ba027a503ad86b65`.
