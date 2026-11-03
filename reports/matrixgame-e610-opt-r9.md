# MatrixGame E610 r9 — оптимизированная native сборка с assertions

Основание: [аппаратный профиль r8](matrixgame-e610-r8-hardware-20261103.md).
R8 ускорил реальные переходные Draw примерно 1.57x, но всё ещё 0.618 presents/s.
В CMakeCache пустой CMAKE_BUILD_TYPE; настоящий build.ninja не имеет -O1/O2/O3.
Native C++ код компилировался без optimization. Это конкретная проблема
сборки, подтверждённая фактическими commands, не только metadata label.

## Изменение

Стандартный current diagnostic builder явно задаёт RelWithDebInfo и flags
-O2 -g -UNDEBUG -ffp-contract=off для C и C++. Оптимизация не включает
fast-math, не отключает exceptions/assertions или resource checks. Каждый
exported compile command проверяется на последний -O2, наличие -UNDEBUG,
отсутствие -DNDEBUG/-DASSERT_OFF/-ffast-math/-Ofast/-fno-exceptions. Обязательны
critical CPU/render/loop/main translation units. Передаётся и fingerprints
compiler_profile с effective build type/C/C++ flags; package gate независимо
повторяет фактический command audit. Настоящий SDK debug deko3dd подтверждается
прежним map/nm gate. Библиотеки SDK не меняются.

Предсобранный dependency Rangers archive остаётся прежним по SHA. Для него не
заявляется ни пересборка O2, ни включение flags из нового CMake. O2 относится
к translation units, построенным текущим diagnostic CMake проектом.
Главная NRO и предыдущие NRO/ZIP не пересобираются и не перезаписываются.
Generated original sources/upstream pins, ресурсы и сохранения не меняются.

## CPU профиль и проверка

Benchmark исполняет настоящий NativeRenderState::ValidateDraw и FixedVertexState
DecodeOne/Prepare: 20000 state checks, 200000 decoded vertices. Workload synthetic
валидных CPU данных; не бой и не GPU эмуляция. На host g++ без LTO, без fast-math:
медиана трёх запусков state validation O0=851994 мкс → O2=81148 мкс (10.50x);
vertex decode O0=22279 мкс → O2=4505 мкс (4.95x). Эти коэффициенты не являются
прогнозом Switch FPS. Входные данные и production sources одинаковы.
Checksum во всех host O0/O2, Windows /O2 и ARM64/QEMU -O2/UBSan:
14249933621601823491. Четыре rejection guards сохранены, NDEBUG запрещён
самим benchmark source. ARM64 type_info visibility workaround прежний,
UBSan/vptr не отключаются.

Отдельные короткие host ASan/UBSan/LSan и ARM64/QEMU UBSan проверки выполнены
с -O2: pool completion/failure ownership, exact uniform bytes, 208808 ordered
draws/3310 submissions на прежних 92 counts, Init GPU handoff и 7 journal
fault cases — PASS. Реран обусловлен новым compile optimization; большие
Windows боевые сценарии и полные регрессии не повторялись. Windows reference
O2 benchmark PASS, неизменённые CPU functional tests Windows из r8 сохранены.

## Граница

R9 HARDWARE PENDING. Пул/телеметрия/таймеры r8 сохранены. Таймаут 120 секунд,
критерий четырёх map frames, шаг времени и transition не ослаблены. Физический
HID/GPU FPS/звук нового r9 требуют нового наблюдения. Standalone entry не
вызывает Campaign/Gov, естественная победа/возврат в Galaxy не проверяются.
[Инструкция](../docs/matrixgame-e610-opt-r9-readme.ru.md).
