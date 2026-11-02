# E2E-5.74 — owning Rangers host session и ранний native Init

Статус: **четыре audio modes host/ASan PASS; real hosted native Init rollback
PASS; owning-host full SDK link PASS**. Полный native Init/map, объединённый
native/deko+Rangers SDK executable и новая diagnostic NRO ещё открыты.
Аппаратного запуска нет.

## Композиция настоящих сервисов

`RangersHostSession` объединяет в одном owner:

1. Scope flags/settings standalone host.
2. `RangersDataSession` с настоящими packages/config/language/mod roots.
3. Оригинальный cache и `TSoundControl` через `RangersAudioSession`.
4. `RangersMusicSession`, только когда музыка включена.
5. `EngineCallbacks` с producer всех 16 оригинальных Robot functions.

Members освобождаются в обратном порядке, включая неуспешный constructor.
Callbacks/их controllers и images закрываются до music workers, audio/cache и
data/packages. `EngineCallbacks::Bound()` позволяет отклонить чужой host binding.
Живые original Rangers renderer device/texture context также отклоняются.
Это standalone host; отдельный MatrixGame deko device не является Rangers D3D
renderer и не подменяется.

`HostSettings` временно выбирает sound/music/RobotSound/RobotMusic и отключает
renderer-specific cache eviction; прежние flags и MusicVolumeScale восстанавливаются
после teardown. Реальные службы не заменены fixtures или no-op функциями.
Caller выбирает ec_file read/private-user roots и держит host до конца original
native Deinit. `host.Table()` передаётся в существующий `NativeGameOptions.services`;
таблица копируется, а callback context принадлежит host.

## Все режимы effects/music

Оригинальный `TSoundControl_Create` создаёт control/lock во всех режимах, но
открывает backend только при `SoundEnabled || MusicEnabled`. Shared-data
конструктор AudioSession теперь допускает fully-muted режим. Legacy отдельный
tree constructor сохраняет прежнее требование включённого sound или music.

Host и ASan/UBSan выполнили все 16 callback fields для комбинаций 00/01/10/11:
sound create/play/query/volume/pan/free, one-shot sound, original AFT render/free,
progress/release и music get/set/takt. В выключенных режимах работают исходные
guards Robot/GR_Sound/GR_Music. При включённой музыке настоящий Vorbis stream
потребляется SDL dummy до 1024 frames.

Fully-muted owner успешно создаётся даже с заведомо несуществующим SDL driver:
audio device не открывается. Активный audio режим с тем же driver возвращает
ошибку, освобождает контекст и восстанавливает flags. Повторное создание с dummy
успешно. Duplicate owner не нарушает действующую привязку.

```text
HOST_SESSION effects=0 music=0 full16=PASS teardown=PASS
HOST_SESSION effects=0 music=1 full16=PASS teardown=PASS
HOST_SESSION effects=1 music=0 full16=PASS teardown=PASS
HOST_SESSION effects=1 music=1 full16=PASS teardown=PASS
HOST_SESSION modes=4 original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS SDL_DUMMY_ONLY NO_NATIVE_INIT NO_SWITCH
```

В этих двух profiles десять новых host TUs инструментированы в ASan варианте;
329 original Rangers dependency TUs повторно использованы без instrumentation.
Слышимого аудиовыхода нет; populated GUI/texture branches не проверены.

## Совместная проверка с оригинальным native Init

`probe_matrixgame_hosted_native_prefix.cpp` создаёт настоящий RangersHostSession,
затем NativeInitRuntime. Выполняется реальный prepared `CGame::Init` до
`Device::ReadCaps`. Там тестовое устройство явно возвращает Unsupported:
оно не выдаёт вымышленные GPU capabilities и не позволяет объявить Init успешным.

В этой точке проверены live original Matrix heap/config/cache и отдельные Rangers
roots; copied service table направляет original CLoadProgress dispatch, real AFT
text/free и real looping sound callbacks. После исключения original native
cleanup освобождает Matrix resources и copied table. Host/context/data/audio
остаются живы: созданный sound можно запросить и освободить после rollback.
Повторный Deinit безопасен. Затем host destructor очищает свои resources/globals.
Полный цикл повторён дважды с ASan/UBSan, без утечек.

```text
HOSTED_NATIVE original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS NO_GPU NO_FULL_INIT NO_SWITCH
```

Первая попытка этой проверки ошибочно использовала установочный Rangers root
для native MatrixGame. Она остановилась раньше caps: отсутствовал
`cfg/robots/data.txt`. Причина была в test setup, не в CFile. Исправленный run
использует раздельные read roots:

- Rangers: настоящая установленная игра с Main/Lang/CacheData/install/packages.
- MatrixGame: отдельный private read tree с неизменёнными upstream
  `MatrixGame/CFG/robots/{data.txt,iface.txt}` и WSL symlink на original robots.pkg.

Эти CFG совпадают SHA256 с upstream. Старые fixture/frozen inputs не менялись.
Первый abort и diagnostic message сохранены в `native-prefix-asan` и
`native-prefix-debug`; финальный успешный run — `native-prefix-final`.
Тест использует ранее проверенные Matrix ASan objects/archives и реальные
Rangers dependencies; весь original engine заново не инструментировался.

## SDK и повторение

Owning-host SDK ELF реально включает RangersHostSession, DataSession, borrowed
audio/music constructors, EngineCallbacks/Bound и все original callbacks.
ELF64 AArch64 PIE; strong undefined = 0; SDK executable **NOT RUN**.
Этот SDK ELF не включает CGame::Init: joint native Init выполнен в отдельном
host prefix test. Совместный native/deko+Rangers SDK link ещё не заявляется.

ELF: 44400024 bytes,
SHA256 `09e52405e6ddebea40d268790d9ad182cb1d4e48c07dd790420fce645e45433d`.
Callbacks archive: 886130 bytes,
SHA256 `e47982138f9f9ad7c20c8e7a4d07ecfc343c9e66cb317f10909e6c66e03b7aba`.

```powershell
python tools/matrixgame_rangers_host_session_checks.py host
python tools/matrixgame_rangers_host_session_checks.py asan
python tools/matrixgame_host_session_sdk.py
python tools/matrixgame_hosted_native_prefix.py
python tools/matrixgame_host_session_audit.py --output reports/matrixgame-e2e574-recheck.json
```

Outputs: `build/matrixgame/e2e574-rangers-host-session/`.
Evidence: `reports/matrixgame-e2e574-rangers-host-session-evidence.json`.
Как E573, рецепты инкрементальные: требуются сохранённые предыдущие проверенные
objects/archives, WSL/SDK и original resources. Для скорости WSL link helper
теперь умеет стейджить дополнительные native archives в Linux `/tmp`; прежние
вызовы без extra_archives сохраняют прежнее поведение.

## Достигнутая цепочка и следующий шаг

Общий host owner, все audio modes, full16 dispatch и его lifetime через ранний
original native Init rollback проверены. Native CFile/private output provider,
StoredStateBackend и copied table integration остаются сохранёнными. Полный Init
ещё не доказан; caps boundary теста находится раньше прежней hardware boundary.

Следующий software шаг: подключить owning host в стандартный diagnostic entry
с явными Rangers/Matrix roots и объединить native/deko SDK target с оригинальными
Rangers dependencies. Затем собрать обновлённую отдельную NRO и продолжать
доступные Init/map проверки. ARM64/QEMU и Windows execution этой новой композиции
**NOT RUN**. GPU/frame/F9/audio/HID/focus/recovery требуют пользовательского Switch.

Аппаратная граница logs2 остаётся ReadParams → ApplyGammaRamp, до map constructor.
Новая NRO не выпускалась. Последняя отдельная E568 NRO сохранена с SHA256
`d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750`.
Аудит подтвердил clean cpp/matrixgame/okgf pins, protected frozen/primary/cumulative
NRO и реальные игровые inputs. Ресурсы и сейвы не изменены; commit/push нет.
