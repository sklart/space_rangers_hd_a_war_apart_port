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

## Проверенная доставка r9

Source commit: `161b6d5a0923969af93762a252f2477db98cdb60`; build ID `161b6d5a0923-cac3d1dc698dae229e1a`.
ZIP: `dist\matrixgame-e610-switch-test-r9-opt.zip` (10154479 байт).
ZIP SHA256: `20474b116b66caa29e5047e37c0d315f2039d2c798f754eb6fdd5e1444cd6a9a`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `4e81a0d7990963c150e7dba31264173cd40b7a279ddbf9d6a06fbd1afb8cc487` | `1240362f9bee19627bbee4b1e366999828b20bd3` |
| matrixgame-e610-interactive.nro | `88554503574b871b84891009bbc0036cfc2f0a9736c332f3fa53ec7885cf4e1f` | `f04fd7840e2efa09c8833b572259e08470848aa4` |

Итог: **SOFTWARE PASS / R9 HARDWARE PENDING**. Оба devkitA64 compile/link,
NRO structure/resources/title/build-ID checks — PASS, strong undefined=0.
Фактические 321 configured C/C++ commands имеют последний -O2 и -UNDEBUG;
без -DNDEBUG/-DASSERT_OFF/-ffast-math/-Ofast/-fno-exceptions. Набор configured
translation units больше реально собранного NRO subset; compile/link целевых
NRO прошли. Старые реальные r8 native FLAGS отвергнуты новым negative gate.
Настоящий deko3dd (23 object members), 0 deko3d, debug error path подтверждены.
697 собственных source SHA совпали с metadata, generated original файлы
совпали с r8/r7/r6/r5/r4. Сверены 199 защищённых прежних файлов/пакетов;
основная NRO, upstream, frozen, игровые ресурсы и saves сохранены.
[Полное evidence](matrixgame-e610-opt-r9-evidence.json).

Host/ARM64 O2 regression results: build/matrixgame/e610-opt-r9-regression/*/1/.
Windows O2 functional tests r8 переиспользованы: production/test sources
не изменились; старый runner hash исторический, его изменение касается только
параметра optimization/output-root. Новый Windows O2 CPU checksum/guards test
исполнен отдельно. Большие боевые сценарии не перезапускались.

Для пользователя: только две NRO из ZIP → `/switch/matrixgame-e610-r9-test/`.
Названия hbmenu: MatrixGame r9 Quick Smoke, затем MatrixGame r9 Interactive.
DATA и три MatrixGame файла остаются на прежних местах. Штатный выход из
Interactive — Minus+Plus, до 120 с после Init. Передать два новых run-каталога
из `/switch/matrixgame-diag/logs/` и наблюдения о скорости/изображении/звуке.
До этого аппаратный FPS нового r9 не измерен; Campaign/Gov/победа не приняты.
Дальнейшие изменения ожидают нового аппаратного профиля.
