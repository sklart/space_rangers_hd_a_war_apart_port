# MatrixGame Switch plan

Observed `MatrixGame.dll` imports: `WINMM`, `d3d9`, `d3dx9_43`, `KERNEL32`, `USER32`, `GDI32`, `ADVAPI32`, `SHELL32`. It is loaded as a separate x86 Windows binary and cannot run on ARM64 Horizon.

## Исходники в проекте

На 2026-10-16 подключён подмодуль [`upstream/matrixgame`](../upstream/matrixgame):

- источник: https://github.com/vladislavrv/MatrixGame.git;
- ветка upstream по умолчанию на момент получения: `refactoring`;
- зафиксированный commit: `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`;
- дата commit: 2025-05-20; сообщение: `Merge pull request #76 from exweilt/refactoring`;
- лицензия движка (`MatrixGame` и `MatrixLib`): GPL-2.0-or-later согласно upstream README; текст GPL v2 находится в `LICENSE` подмодуля;
- исходники сохранены без локальных изменений. Существующие pins C++ и OKGF сохранены.

Получение зафиксированной версии в новом checkout:

```sh
git submodule update --init upstream/matrixgame
```

Gitlink фиксирует точный commit, поэтому обычный `submodule update` не переводит зависимость автоматически на новую вершину ветки. Обновление версии должно быть отдельным проверяемым изменением.

## Состояние переноса

**SOURCES AVAILABLE / SWITCH INTEGRATION PENDING.** Отсутствие исходников больше не является препятствием. Добавление подмодуля пока не включает планетарные бои в NRO.

Upstream использует C++20 и CMake, поддерживает Windows x86 с MSVC/MinGW и собирает DLL либо отдельный EXE. Его CMake требует DirectX 9; `ThirdParty/CMakeLists.txt` получает zlib и libpng. Эта конфигурация не подключена к текущей сборке Switch.

Дальнейшие шаги:

1. Сопоставить `MatrixGame/src/MatrixGameDll.hpp` и экспорт `GetRobotInterface` в `MatrixGame/src/MatrixGameDll.cpp` с интерфейсом `upstream/cpp/src/Robot.cpp` и DLL установленной версии 2.1.2500. Совпадение ABI и игрового поведения пока не проверено.
2. Отделить Win32, Direct3D 9/D3DX9 и оконный цикл от логики движка; подготовить графический, звуковой и input-адаптеры для Switch.
3. Проверить зависимости от 32-битных указателей, структур, соглашений вызова и сериализации при переходе на ARM64.
4. Подключить адаптированный движок к загрузке и завершению планетарного боя; затем проверить совместимость ресурсов и реальный запуск на консоли.

Windows DLL остаётся справочным бинарным эталоном и не загружается на ARM64 Horizon. Локальная загрузка исходников, сборка host/ARM64 и аппаратная работоспособность — отдельные этапы проверки.

## E2E-5.8: MG-0 — проверенный интерфейс и границы переноса

Статус: **MATRIXGAME INTEGRATION PENDING**. Подмодуль не включён в NRO.

Сопоставлены `MatrixGame/src/MatrixGameDll.hpp`, `.cpp`, `MatrixLib/Base/BaseDef.hpp`
и `upstream/cpp/src/types/Robot.hpp`, `Robot.cpp`, `types/aGalaxyStruct.hpp`.
`GetRobotInterface` имеет C export/cdecl; таблица содержит stdcall Init, Deinit,
Support и Run. Run вызывает Init → RunGameLoop → SaveResult → SafeFree,
возвращает 0 (выход), 1 (меню), 2 (поражение), 3 (победа), 4 (отмена), +100 (ошибка).
Порядок callbacks совпадает: звук/create/destroy/play/query/volume/pan/getters,
render text/clear, progress, takt, begin, music volume get/set. Названия
PlayMusic/ReleaseTextures в переводе основной игры отличаются от m_Takt/m_Begin:
совпадение позиции не устанавливает семантическую совместимость.

Публичный заголовок MatrixGame реально скомпилирован MSVC 19.44 для **Windows x86**
с `tests/matrixgame_x86_abi_probe.cpp`; все static_assert прошли. Это проверка ABI,
не сборка/запуск движка и не сравнение поведения с установленной Windows DLL.

| Тип/поле | Windows x86 | Требование ARM64 |
|---|---:|---|
| pointer / int / long / wchar_t / bool | 4 / 4 / 4 / 2 / 1 байт | native pointers 8; числа fixed-width; текст char16_t |
| SMGDRobotInterface | 16 байт; Run offset 12 | нативная таблица 4 указателей; не читать как x86 blob |
| SMGDRangersInterface | 64 байта / 16 callbacks | 16 нативных function pointers; явные сигнатуры |
| SRobotsSettings | 56 байт; BPP offset 16; VSync offset 52 | поля FDirect3D/FD3DDevice — long, а не безопасные указатели; заменить backend handles |
| SRobotGameState | 24 байта / 6 int32 | перевод в TPlanetBattleStatistics по полям; сохранить знак времени |
| SMGDRangersInterfaceText | 20 байт | m_Image DWORD нельзя использовать для 64-битного указателя; token/реестр владельцев |

В заголовке MatrixGame нет pack pragma: Windows x86 использует обычное выравнивание
(максимальная фактическая граница полей здесь 4). Основной перевод задаёт pack(1),
но указанные x86 смещения совпадают. На ARM64 нельзя memcpy таблицы, настройки
или текстовые результаты между этими layout. `dword=unsigned long` и
`wchar=wchar_t` из BaseDef на LP64 меняют ширину. Cdecl/stdcall Windows x86
заменяются единым нативным ARM64 ABI. Размеры исходных структур на ARM64 не
являются форматом сохранения. Владелец текста освобождает изображение через
парный FreeText; звук должен возвращать token, а не усечённый указатель.

## Зависимости и архитектура адаптера

| Подсистема/исходники | Классификация | Действие |
|---|---|---|
| MatrixGame/src/Logic, правила боя, AI/pathfinding | логика — переносима | сохранить алгоритмы, порядок объектов и RNG; убрать зависимости через интерфейсы |
| MatrixLib/Base, CFile, Pack, Registry, CHeap, оконный цикл | Windows API — требуется адаптер | read-only game root, явный user root, native handles, настройки без Registry |
| MatrixLib/3G: D3DControl, DeviceState, BigVB/BigIB, Texture, ShadowProj/Stencil | Direct3D 9 — требуется новый backend | геометрия/индексы/текстуры, blend/depth/stencil/culling, материалы и эффекты |
| DATA/robots.pkg, CFG/robots.dat | ресурсы — переиспользуются | только локальные законно полученные ресурсы, без коммита и модификации |
| MatrixGameDll, Robot.cpp, callbacks | интерфейс главной игры — требуется мост | native Run/Init/Deinit/Support с переводом полей и явным временем жизни |
| ThirdParty zlib v1.2.11, libpng v1.6.37 | сторонние библиотеки | upstream ExternalProject закрепляет теги; для Switch системные порты после совместимых decode checks |
| winmm, d3d9, d3dx9, DxErr | платформенные библиотеки | заменить таймер/аудио и D3DX math/image helpers; не тащить Windows DLL |

Для первого прототипа выбран **deko3d** как кандидат нативного Switch backend.
Это архитектурное решение для MG-2, не подтверждение готового renderer.
Нужны отдельные проверки toolchain, шейдеров и совместимости форматов. Уже
существующий SDL2 RGB565 renderer основной игры не покрывает D3D9 pipeline.
Альтернатива EGL/OpenGL рассматривается только при измеренной непригодности
deko3d; предварительно переписывать весь движок не требуется.

PlatformServices должен предоставлять Files, Clock, Synchronization, Input,
Audio, Renderer и Text. Роботная логика обращается к этим границам; native
MatrixGameBridge владеет контекстом боя и выдаёт результат основной игре.
Вход: карта, язык/тексты UTF-16, настройки, данные боя. Выход: result code,
шесть счётчиков, возврат звука/renderer/input основному экрану. Никаких
автопобед: отсутствие backend означает недоступный бой с явным статусом.

## MG-1 — самостоятельный Windows x86 эталон

На этом компьютере выполнена конфигурация pinned source через VS 2022 Win32,
отдельно от upstream, в ignored `build/e2e58/matrixgame-win32`. Компилятор x86
обнаружен. CMake остановился: `Could NOT find DIRECTX9 (missing:
DIRECTX9_LIBRARIES DIRECTX9_INCLUDE_DIRS)`. Полная host-сборка и запуск карты
**NOT RUN**; ABI compile не заменяет этот gate. SDK автоматически не устанавливался.

После установки SDK: в отдельном build directory собрать EXE с
MATRIXGAME_BUILD_DLL=OFF, MATRIXGAME_CHEATS=OFF,
MATRIXGAME_PKG_BRING_FROM_GAME=OFF; предоставить read-only ресурсы отдельно.
Проверить старт, карту, движение/выбор/стрельбу робота, победу/поражение/отмену
и завершение процесса. Отдельно собрать DLL и проверить callbacks с исходной
Windows игрой. Сохранить SHA EXE/DLL, карту, входные настройки и итог боя.

## MG-2 — перенос платформенного слоя

1. Вынести Win32 window/input в SDL/HID adapter; разделить курсор, клавиши,
   сенсор и focus. Ввести монотонные таймеры без изменения шага симуляции.
2. Вынести файлы/пакеты, потоки/события и настройки; проверить ошибки открытия,
   short read, OOM и отмену без зависаний. Не изменять оригинальные ресурсы.
3. Прототип deko3d: одна исходная геометрия, текстура, управляемая камера,
   depth buffer и корректное освобождение. Артефакт и кадр на физической Switch.
4. Добавлять состояния D3D9, материалы, ландшафт, shadows/effects по эталонным
   кадрам. Отдельно адаптировать системный текст и RGB/alpha layout.
5. Адаптировать аудио через native callbacks; сохранить sound groups, looping,
   volume/pan и корректный возврат музыки. Gate: самостоятельный реальный бой.

## MG-3 — встраивание

После MG-1 и MG-2: связать статически native MatrixGameBridge, передать состояние
и карту, приостановить основной game loop штатным способом, передать управление,
получить результат, восстановить renderer/audio/input и вернуться на правильный
экран основной игры. Проверить победу, поражение, отмену, повторный вход и ошибку
ресурса. Финальный gate — реальный планетарный бой на Switch с управлением
роботами и корректным результатом в Space Rangers HD.

## A8 DDS: соответствие настоящему D3D9 sampler

Исправлено разворачивание alpha-only DDS в native RGBA8: RGB равен нулю,
alpha сохраняется. Это подтверждено настоящим D3D9 draw/readback в Windows:
HAL на адаптере vendor 0x10de и отдельный программный REF. Для всех 256 alpha
отрисованы 1024 пикселя A8 и 1024 пикселя эквивалентной RGBA8-текстуры в каждом
режиме: different_pixels=0, unexpected_a8_pixels=0, unexpected_rgba_pixels=0.

Опциональный эталон собирается целью `matrixgame-a8-d3d9-reference` в
`port/matrixgame/native-probe`, затем запускается отдельно:

```text
matrixgame-a8-d3d9-reference.exe --hal
matrixgame-a8-d3d9-reference.exe --ref
```

Это реальный Windows D3D9 draw/readback. HAL и REF явно разделены; NULLREF
не используется. Проверка не подтверждает исполнение deko3d или работу Switch.
Независимый DDS oracle обновлён по измеренной семантике A8. Игровые ресурсы
сохраняются без изменений; исправление не устанавливает причину зависания
основной игры после «Готово».
