# MatrixGame: нативная таблица engine callbacks — 18.10.2026

Статус: интерфейс SMGDRangersInterface подключён к RangersBridge.
Это не CGame::Init/Run, не выполненный бой и не интеграция результата в кампанию.

## Исходники и контракт

Подготовка проверяет 21 исходный input. Добавлены оригинальные MatrixGameDll.hpp
и MatrixGame.h из неизменённого submodule 218115a72fa9c5d43269f4ca84a8596a0bcb82f1.
Их LF-normalized SHA-256:

- MatrixGameDll.hpp: f3ae60f5b1945b8ecae9e99749ee35ea3ce61a9112f4f7c8c90ffddf904147b0
- MatrixGame.h: 65b42d2d1f647cc328ec7d50ba983ad8e7fd60daccf82756796c3657117d018e

Windows DLL calling conventions/import attributes удалены для внутреннего C++ ABI.
32-битные sound/image ID сохранены как порядковые номера, а borrowed graphics
handles стали void*. HINSTANCE/HWND представлены непрозрачными контекстами;
функций Windows эта подготовка не реализует. Все поля default settings инициализированы,
в том числе исходно неинициализированные handles и размеры экрана.
GetRobotInterface остаётся объявлением без подставной реализации/результата боя.

EngineCallbacks предоставляет таблицу из всех 16 оригинальных callback signatures.
Таблица имеет статическое время жизни; каждый вызов удерживает shared context
до возврата. Одновременное второе подключение отвергается. После отключения новые
вызовы отвергаются, уже начатые завершаются с прежним владельцем ресурсов.
Подключать/переподключать таблицу следует при остановленном единственном engine.
Код callbacks и игровые services должны пережить выполняющиеся вызовы.
Диспетчеризации на UI thread здесь нет.

wchar_t преобразуется в явные UTF-16 units; игровые callbacks получают изменяемые
копии строк. Nullable clip сохраняется. При ошибке вывода текста выход обнуляется,
частично созданные ресурсы освобождаются; отрицательные размеры/pitch отвергаются.
Шесть signed полей статистики и все settings копируются по полям.

## Проверка

- MSVC x64 Release: 10/10 CTest PASS. Устранён выбор MSVC вложенного
  types/classes.hpp вместо runtime/classes.hpp посредством раннего явного
  подключения runtime_support.hpp из translation unit; upstream не менялся.
- Linux ASan/UBSan: 10/10 PASS; Linux TSan: 10/10 PASS, включая повторные
  прогоны после финальной правки порядка includes.
- ARM64 Linux/QEMU: callback contract PASS, 16 engine slots, 800 concurrent
  sound cycles, resources_remaining=0, pointer_bytes=8, battle_executed=false.
- devkitA64: полная сборка SDK targets адаптера и пяти CPU link probes PASS.
  SDK ELF не запускались; это не аппаратная проверка.
- Аудит effective sources: fixed_site_regression_failures=[], остаётся
  REVIEW_NATIVE_SEMANTICS=1. Это inventory, не доказательство всех игровых сценариев.

Финальный SDK ELF адаптера:

- build/mg-engine-boundary-switch/matrixgame-engine-api-link-probe.elf
- SHA-256: 1a6ef8654c00e0b68af5d65f2d500a35f52d61da8093dbca3b6503ef164a3565
- GNU Build ID: 44d7f43da358fac6218d0467fefd11e850cfdc2d
- nm --undefined-only: пусто.

Контракт использует контролируемые producers с настоящими типами игровых callbacks.
Проверены Unicode, forwarding, stale IDs, partial failures, nullable clip,
повторное подключение и отключение при заблокированном выполняющемся callback.
Работа реального audio/text renderer основной игры этим тестом не подтверждается.

Воспроизведение host проверок: команды native CMake в port/matrixgame/README.md.
ARM64: bash tools/matrixgame_engine_api_arm64.sh.
SDK: port/matrixgame/native с switch-toolchain.cmake, BUILD_TESTING=OFF,
CORE_BUILD_LINK_PROBE=ON, BRIDGE_BUILD_LINK_PROBE=ON. DEVKITPRO должен указывать
на фактический SDK root; локальный Windows путь C:/devkitPro.

## Полный движок: FAIL / остаётся работа

tools/matrixgame_engine_prepare.py создаёт disposable копию 232 source files,
проверяет pin/чистоту оригинала, накладывает настоящие portable headers/core inputs
и сохраняет SHA каждого подготовленного файла. Windows launcher не является native
entry. WinMain body удаляется из compilation view; MatrixGameDll.cpp исключён.
Остальные методы игры не заменяются заглушками.

port/matrixgame/native-engine содержит отдельный opt-in static compilation target:

```sh
cmake -S port/matrixgame/native-engine -B build/mg-native-engine
cmake --build build/mg-native-engine --target matrixgame-native-engine --parallel 2
```

Configure PASS; полная компиляция FAIL на MatrixLib/3G/VectorObject.hpp:8:
fatal error: d3d9.h: No such file or directory. Готовность CPU library не означает
готовность этого target. Требуются реальный перенос graphics/resource lifecycle,
компиляция остальных зависимостей, native entry, Init/Run и возврат в кампанию.
Static archive также сам по себе не доказывает полной линковки движка.

NRO этого этапа не выпущена. Кандидат стабилизации 3772031 не изменён.
Причина аппаратного замершего экрана после «Готово» остаётся ROOT CAUSE UNCONFIRMED.
Физическая Switch, MG-2…MG-5 и публикация скачанного master CI artifact остаются открыты.
