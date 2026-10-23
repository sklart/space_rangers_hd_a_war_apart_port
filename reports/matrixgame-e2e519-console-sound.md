# E2E-5.19 — настоящие Console/Sound constructor dependencies

23.10.2026. Продолжение `9c54546`, локальный master без push. Полный Map TU уже
скомпилирован, но настоящий ctor/dtor ещё не линковался. В этом шаге подключены
полные исходные DevConsole.cpp и MatrixSoundManager.cpp в общий native-cache.
Новый самостоятельный CPU/game prototype и фиктивная карта не создавались.

Единственный baseline четырёх ближайших TU показал: весь MatrixSoundManager.cpp
уже COMPILE PASS, Console останавливается на Win32 VK constants/timeGetTime,
DebugInfo требует SDK font, Transition — GDI capture и device draw.

В Console заменены только девять VK constants (Shift дважды) и две выборки
elapsed time в исходной команде TRACESPD. Сохранены все обработчики, оригинальная
таблица команд, Keyboard/ShowHelp/Takt, конструктор и деструктор. Новые constants
сверены C++ static_assert с настоящим Windows SDK. BenchmarkMilliseconds читает
настоящий steady clock с исходным unsigned-32 wrap; эта команда не меняет таймер
игры и сама в тесте не исполнялась. RNG/trace benchmark не запускался.

Sound TU и RoadNetwork.cpp относительно принятого source view не менялись.
RoadNetwork подключён как уже перенесённая зависимость, без повторения его suites.
Function/data sections у navigation согласованы между CMake и ручным recipe.

В существующем bootstrap после настоящих game defaults:

- HNew/HDelete на настоящем g_MatrixHeap исполняют original CDevConsole ctor/dtor;
  проверено исходное inactive состояние и SetActive(false), без поддельного map;
- original CSound::Init, IsSoundPlay(SOUND_ID_EMPTY), StopPlayAllSounds и Clear
  исполняются при реальном nullptr g_RangersInterface, затем повторяются Init/Clear;
- g_MatrixMap не устанавливается, console commands/Keyboard/draw не исполняются.

Звуковой callback не заменяется fixture-успехом: original IsSoundPlay возвращает
false. **Аудио недоступно**. Это выполнение sound bookkeeping/nullable-interface
lifecycle, не проверка воспроизведения, PCM/backend handles или Clear с живыми
голосами. Освобождение live audio и полный console input остаются NOT RUN.

Первый host link выявил отдельную особенность GCC ASan: регистрация global
command table удерживала все невызываемые handlers и их ещё неперенесённые
Trace/CalcVis/DebugInfo::T зависимости. Не удалялись ни команды, ни методы игры.
Изменение только `--param=asan-globals=0` для Console TU сняло эти ссылки.
nm сравнение подтверждает: таблица осталась в обоих object files, исчезла только
ASan global registration; Address reports и UBSan handlers остались.

Ограничение важно: **ASan global redzones этой TU не проверяются**. Stack/heap
Address и UBSan включены, Sound globals не исключены. В CMake флаг ограничен
GNU+ASan конфигурацией; Release/SDK его не получают. Прежние оговорённые
исключения vptr в Logic/Map остаются. Первые failed outputs сохранены.

Фактические gates: host ASan/UBSan/leaks и ARM64/QEMU — общий bootstrap вернул0
в двух сессиях; SDK — COMPILE/LINK PASS отдельного ELF с retained Console/Sound
symbols и strong undefined=0; MSVC x86 — полная Console COMPILE PASS и SDK key
contract COMPILE PASS, Windows console runtime NOT RUN. Это не full-map link.

Отдельный force-retain настоящих CMatrixMap ctor/dtor теперь даёт LINK FAIL
только на трёх symbols: CMatrixDebugInfo ctor/dtor (MatrixDebugInfo.cpp,
исходный D3DXCreateFontW) и CTransition::Clear (MatrixTransition.cpp, unwind).
Прежние Console/Sound/RoadNetwork symbols исчезли из этого списка.
Карта, CGame::Init, CFormMatrixGame и бой по-прежнему NOT EXECUTED.

Свежие production prepare и CMake configure/generate сверяют весь source view
и реальные compile recipes. Результаты, commands, source/object/log hashes,
ASan comparison и next blockers: matrixgame-e2e519-console-sound-evidence.json.
Сборка до коммита, build_dirty=true. Frozen main NRO/ELF/ZIP,107 аппаратных входов,
пользовательские сейвы и ресурсы сохранены; новая NRO не создавалась.
Источники аппаратного повторного текста/двух ENOENT/freeze карты остаются OPEN.
