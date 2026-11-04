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
Median 196263 → 87682 мкс (−55.33%, 2.24×) на 10 млн vertices.
Gather frozen: 358563/358832/369774; candidate 276393/271380/307078.
Median 358832 → 276393 мкс (−22.97%, 1.30×) на 4096 draws.
Checksum 5258787986328867328 одинаков во всех шести runs. Последний candidate
run медленнее остальных; даже он быстрее всех frozen gather runs. Выигрыш
подтверждён обеими сериями, но проценты относятся только к CPU probe.
Новых аппаратных измерений Init/Draw/FPS нет.
