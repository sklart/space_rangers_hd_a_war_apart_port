# MatrixGame: эталон и применимость исправлений

Статус: PARTIAL. Windows x86 карта/управление/бой/статистика проигрыша проверены.
Результат кампании и Switch backend ещё не проверены.
Pinned `vladislavrv/MatrixGame`: `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`, не изменён.
Для read-only сравнения скачаны отдельные bare-копии в ignored build/matrixgame:
`murgesku` HEAD `06e4d327a050490eb7f52213c5fcfa7479464394`,
`twoweeks` HEAD `10deeb427f7a3daa7b5a5a361020264f3b17087f`.
Они не подключены вместо pinned submodule и не сливались с ним.

## Сопоставление runtime-изменений murgesku

| Изменение / источник | Применимость к pin | Риск / решение | Проверка |
|---|---|---|---|
| [06e4d327: тип головы](https://github.com/murgesku/MatrixGame/commit/06e4d327a050490eb7f52213c5fcfa7479464394) | Ошибка присутствует: CConstructor.cpp:1479 задаёт MRT_WEAPON; далее SetPrice на 1634 использует этот тип | Адресный patch MRT_HEAD подготовлен, pin сохранён; не применять весь fork | git apply --check, полная x86 сборка и native D3D AI template test PASS; before: 159 invalid types, 23 price/total component mismatches; fixed: 0/0/0 |
| [2d42599: trace >2GB](https://github.com/murgesku/MatrixGame/commit/2d42599a3df9f0c095a1b0d4a208284e9bac2f0c) | Pin MatrixMap.hpp:53 уже использует uintptr_t | Не переносить DWORD: на ARM64 он усечёт адрес. Сохранить uintptr_t | Код просмотрен; аппаратная проверка trace ещё PENDING |
| [ac21bc2: границы pathfinding](https://github.com/murgesku/MatrixGame/commit/ac21bc264145afb964b829e4f3fdc8e21f6e7c29) | В pin MatrixLogic.cpp:777–879 уже есть 11 guards other_cnt >= 200 перед соответствующими записями | Не дублировать патч; сохранён ранний отказ поиска | Сопоставлены все 11 мест; нагрузочный бой ещё PENDING |
| [7e08274: CHEATS_ON](https://github.com/murgesku/MatrixGame/commit/7e08274dd6bfa18963d6cee37ebdab152f852fff) | Pin имеет CMake MATRIXGAME_CHEATS и CHEATS_ON | Для эталона явно задан OFF; это конфигурация, не исправление gameplay | CMake cache/исходник; поведение клавиш ещё PENDING |

Остальная история murgesku: `f5f48a5` переносит каталоги/сборочную структуру,
`c130e31` добавляет информацию CMake, `10785b9` меняет кодировку,
`d59ac68` / `d12fb59` относятся к 3ds Max exporter, `50af014` — README,
`2f7f953` — исходная база. Они не принимаются как runtime bugfix без отдельной
проверки; редактор 3ds Max не нужен для исполнения существующих игровых карт.
Таблица не объявляет весь diff forks семантически эквивалентным.
Исторический twoweeks использует структуру Programm/MatrixGame, поэтому сравнение
путей выполняется с учётом переноса, а не по совпадению имён каталогов.

## Windows x86 и SDK

MSVC 19.44.35229.0 x86 + Windows SDK 10.0.26100.0.
Первый configure без DirectX SDK ожидаемо остановился на DIRECTX9.
Затем SDK получен по [официальной ссылке Microsoft](https://www.microsoft.com/en-gb/download/details.aspx?id=6812):
599455936 байт, SHA-256 `705271dc83bfee54d9b94e028426e288d5f070784b7446d164f48ecfbb2a02cb`.
Authenticode: Valid, Microsoft Corporation. Установщик не запускался.
Include, Lib/x86 и лицензионные тексты извлечены 7-Zip в ignored build/matrixgame/directx-sdk.
Системные каталоги/реестр и исходная установленная игра не изменены.
Повторный configure с этим Include прошёл. Baseline DLL и EXE собраны из pinned
исходников настоящим MSVC x86, MATRIXGAME_CHEATS=OFF. DLL: 2187264 байт,
SHA-256 `a7d0b99c9251630c354f6dd5b13aafe5c7070af90641f34212cb6848b9372a19`.
EXE: 2109440 байт, SHA-256
`435536c2b54151776f01f24058b36a1794cfb8a29bdffd9cfe04a48df59df773`.
Логи: build/matrixgame-baseline-build.log и build/matrixgame-baseline-exe-build.log.
Ресурсы robots.pkg скопированы только в отдельный ignored build; исходная установка
не изменена. Сборка не подтверждает запуск карты или бой.

Воспроизводимая ABI-проверка `tests/probe_matrixgame_abi_x86.cpp` включает настоящий
MatrixGameDll.hpp и BaseDef.hpp, компилируется через vcvars32 + cl /std:c++20 /EHsc.
Проверены long/dword=4, wchar=2, pointer=4, settings=56, statistics=24,
robot interface=16, callbacks=64, text image=20 байт, ключевые offsets и типы
cdecl/stdcall. Результат: build/matrixgame/abi-x86.json.
ABI layout PASS. Дополнительно реально загружена собранная DLL, вызван её
GetRobotInterface и проверены четыре ненулевых function pointer, затем DLL выгружена.
Это проверка экспортной таблицы; Init/Run/Support и бой не выполнялись.
Она не доказывает ARM64 совместимость. На первоначальном этапе node_repl отсутствовал; 18.10 инструмент стал доступен,
GUI-проверка выполнена через computer-use, подробности ниже.

Для будущего bridge нельзя reinterpret_cast эти структуры: в MatrixGame
SRobotsSettings FDirect3D/FD3DDevice — x86 long, sound callbacks возвращают dword,
text image хранит DWORD image handle. В восстановленном Robot.hpp соответствующие
поля/результаты уже содержат нативные указатели и packed layout.
GetRobotInterface объявлен cdecl в движке; main wrapper использует StdcallProc
без параметров. Все callbacks движка stdcall на x86; ARM64 адаптер должен явно
разделять игровые ID, borrowed pointers, UTF-16 и ownership изображения.
Native bridge пока не реализован.

## Границы распространения

NOTICE.md уже фиксирует GPL-2.0-or-later для MatrixGame/MatrixLib и отсутствие движка
в Switch NRO. Copyright/license headers pin сохранены; patch ссылается на автора.
Оригинальные robots.pkg/dat, SDK, частные CFG и сборочные DLL не добавляются в Git
или CI artifacts. Окончательная проверка лицензирования совместно с выбранным GPU
backend и всеми зависимостями остаётся частью MG-0 перед интеграцией/публикацией.


## Проверка отдельной адаптации head fix

В build/matrixgame/patched-source создан git archive точного pin, после git apply --check
применён только port/matrixgame/patches/ai-head-module-type.patch. Проверено побайтовое
совпадение CConstructor.cpp с исходным файлом после ровно одной замены MRT_WEAPON на MRT_HEAD.
Ни один pinned исходник не изменён. Полная Release DLL этой копии собрана MSVC x86:
build/matrixgame-head-fixed-build.log.

Попытка связать CPU-only fixture с настоящими MatrixGameInternal и MatrixLib успешна,
но выполнить LoadAIRobotType не удалось: до него CMatrixMapLogic конструирует
CMatrixCamera/console и требует D3DXCreateFontW с готовым D3D device.
После добавления обработки исключений проба вернула код 10 с сообщением
Failed to create font with D3DXCreateFontW(), build/matrixgame/ai-head-fixed.stderr.
Первая необработанная попытка завершилась 0xc0000409; это отказ неполного test setup,
а не доказательство нового игрового краша. Не применяется raw storage/reinterpret_cast
вместо конструктора карты и не подменяется настоящий loader.
Fixture осталась в ignored build; runtime выбор/цена головы и бой всё ещё PENDING.

Patched DLL SHA-256: `4fcec09ec85c389056dd5c8a974e9f004befafcd8dceeadd024f9200c5c11f08`.

Повторная ABI/export проба на patched DLL PASS: настоящая DLL загружена,
GetRobotInterface вернул четыре ненулевых callback, затем DLL выгружена.
build/matrixgame/abi-head-fixed.jsonl. Init/Run/Support и бой не запускались.


## Настоящий Windows x86 бой (18.10)

Baseline EXE SHA-256 `435536c2b54151776f01f24058b36a1794cfb8a29bdffd9cfe04a48df59df773`,
CHEATS=OFF, оригинальный DATA/robots.pkg, CFG из pinned MatrixGame. Ресурсы подготовлены
в отдельном ignored Release/CFG и Release/DATA, исходная установка игры не изменена.
Visual Studio разместила EXE в Release, а ресурсы CMake — на уровень выше; runtime
layout исправлен только в build. Запуск через sky.launch_app завершался до окна
с 0xc0000409. Запуск того же EXE установленным x86 cdb с рабочим каталогом Release
дошёл до настоящей карты. Относительный static logger test.log зависит от launch cwd;
точный стек раннего выхода sky-launch в этом проходе не получен, root cause не объявлен.

Через computer-use реально выполнены: подтверждение Go! Go! Go!, выбор робота,
правый click на местности и его перемещение, выбор шести роботов рамкой,
приказ через minimap, смена камеры, A + цель атаки. На последовательных снимках
видны выстрелы, снижение HP с 310 до 278, гибель отряда (6→5→4→0) и работа AI.
Затем использовано штатное меню «Сдаться» и подтверждение. Получена настоящая
«Статистика боя», Luna, время 00:03:53. Для жёлтой стороны отображались
0 построенных / 9 уничтоженных роботов, 0/0 турелей, 0 зданий. Это значения
экрана движка, не искусственно заданный результат. Screenshot показан в чате;
попытка сохранить его в build через node_repl получила EPERM, локального image-файла нет.
После подтверждения статистики окно исчезло. cdb завершился с кодом 1, который
сам исходный WinMain возвращает и на обычном выходе. В debug output есть first-chance
C++ exceptions, но UI-маршрут дошёл до статистики и закрытия; не заявляется отсутствие
всех внутренних исключений. Лог: build/matrixgame/x86-baseline/matrixgame-baseline-startup-debug.log.

Это реальный самостоятельный бой на PC, не Switch backend, не победа и не
проверка передачи SRobotGameState через DLL обратно в кампанию. Звук здесь отдельно
не измерялся. GUI-маршрут выполнен без изменения baseline EXE и без подмены RNG.

## Head fix: native before/after на D3D-карте

Первоначальная CPU-only fixture заменена новым integration probe:
`tests/probe_matrixgame_ai_templates_x86.cpp` вызывает настоящий CGame::Init с seed=12345,
создаёт D3D карту, читает CFG/robots data и проверяет уже загруженные AI-шаблоны.
Проверяются тип головы, четыре компонента её цены по реальной g_Config и сумма
ресурсов chassis/armor/weapons/head. Затем выполняется штатное освобождение движка.
L3GRun и battle loop в этой отдельной пробе не выполняются.

| Точная строка loader в disposable source | templates / with_head | invalid_types | invalid_prices | invalid_totals | exit |
|---|---:|---:|---:|---:|---:|
| Исходная MRT_WEAPON | 159 / 10 | 159 | 23 | 23 | 3 |
| Адресная MRT_HEAD | 159 / 10 | 0 | 0 | 0 | 0 |
| Восстановленная MRT_HEAD после negative control | 159 / 10 | 0 | 0 | 0 | 0 |

Это проверка настоящих функций loader/SetPrice на реальной конфигурации; исходные
типы/цены не мокируются. Изменялась только одна строка в disposable CConstructor.cpp,
сборки выполнялись последовательно; pinned submodule остался чистым.
Отчёты build/matrixgame/ai-head-native-{before,fixed,restored}.jsonl,
хеши probe/source/robots.pkg — build/matrixgame/native-head-provenance.json.
Воспроизводимая CMake-цель и команды: port/matrixgame/native-probe/CMakeLists.txt,
port/matrixgame/README.md. Новый wrapper configure прошёл. Первая полная сборка в глубоком каталоге
упёрлась в MSBuild FileTracker FTK1011 во вложенном libpng TryCompile path.
README теперь использует короткий build/mg-native. Повтор на свежем git archive pin
с одной строкой head patch завершён PASS: build/matrixgame/native-probe-short-build.log.
Именно tracked CMake wrapper + tracked probe скомпилированы и запущены: 159 шаблонов,
10 с головой, invalid_types/prices/totals=0/0/0, exit=0.
Отчёт build/matrixgame/native-probe-wrapper-run.jsonl, SHA в native-head-provenance.json.


## Продолжение MG-2

Первый native callback/ownership adapter реализован и проверен отдельно от NRO
3772031: все 16 callbacks, settings/statistics, host sanitizers, ARM64/QEMU и
devkitA64 compile-only. Это заменяет раннюю отметку о полном отсутствии native
bridge, но не закрывает MG-2. Engine Run, реальный audio/text handoff и GPU ещё
не подключены. Подробности: matrixgame-native-bridge-20261018.md.
