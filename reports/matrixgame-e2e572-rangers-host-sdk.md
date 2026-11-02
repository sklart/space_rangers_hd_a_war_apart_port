# E2E-5.72 — полный SDK link Rangers services и MatrixGame callbacks

Статус: **full host-services SDK link PASS; matching host/ASan adapter execution
PASS**. В отличие от E569–E571, новый ELF действительно включает producer,
audio/music owners, EngineCallbacks и все 16 оригинальных Robot functions.
Это программная проверка отдельной цепочки сервисов. Native CGame::Init, deko3d
и Switch этим не запускались; новая NRO не создавалась.

## Build-цель и зависимости

В `port/matrixgame/native/CMakeLists.txt` добавлена отдельная опциональная цель
`matrixgame-rangers-host-link-probe`. Она включается через
`BRIDGE_BUILD_HOST_LINK_PROBE=ON` и требует три явных SDK archive paths:
`BRIDGE_RANGERS_HOST_ARCHIVE`, `BRIDGE_RANGERS_OKGF_ARCHIVE`,
`BRIDGE_RANGERS_SOFTFLOAT_ARCHIVE`. Без этих входов configure завершается ошибкой.
Стандартные devkitA64/libnx и portlibs выполняют обычный compile/link.

`tools/matrixgame_rangers_host_sdk.py` готовит оригинальные Rangers dependencies
из сохранённого `port/switch/build/e2e-game-perf515`. Старый build output не
перезаписывается. Из 380 objects исключены:

- `upstream/program.o`: старая точка входа полной Rangers игры;
- `platform/package.o`: общий package-reader предоставляется единственной
  реализацией из `matrixgame-native-core`, собранной из того же source file.

320 объектов повторно используются после проверки compiler version/flags и GCC
depfiles; 58 устаревших объектов пересобраны в отдельном `fresh` каталоге. Повод
пересборки — изменённые platform headers и два platform source files. Новые
depfiles учитываются при дальнейшей проверке closure; cache проверяет SHA256
всех записанных dependencies, flags и private object. Архив содержит 378 objects.
Оригинальные объекты и headers закреплены SHA256 в evidence.

При первом link выявлены дубли package reader и отсутствующие OKGF symbols.
Рецепт исправлен без удаления обязательных операций: package reader теперь один,
а настоящие OKGF и softfloat archives включены в rescan group вместе с native
libraries, SDL2/Vorbis/PNG/JPEG/zlib и libnx. Неуспешные результаты/логи сохранены
в `result-attempt-*.json` и отдельных build logs; первая archive тоже сохранена.

## Реальное исполнение адаптера на host

Новый `tests/probe_matrixgame_rangers_host_link.cpp` загружает настоящие
INSTALL package list, CacheData.dat, Main.dat и явно выбранный Rus/Lang.dat,
создаёт AudioSession → MusicSession → EngineCallbacks. Затем вызывает все
16 полей настоящей `SMGDRangersInterface`:

- nine sound operations: create/play/query/volume/pan/free и одноразовый sound;
- AFT render/free через nullable-clip adapter и настоящий Font.2Normal;
- progress и release в original standalone no-main-device/empty-texture ветках;
- music volume get/set и music takt через оригинальную Music.Robot playlist.

Настоящий Vorbis stream потребляется до 1024 кадров SDL dummy. Проверены raster
extent/pixels pointer, сброс text image после free и sound value round trips.
После закрытия scopes проверено отсутствие music/sound/cache/config globals.

Обычный host и ASan/UBSan profiles завершились:

```text
REAL_HOST_ENGINE full16=PASS text=PASS sound=PASS music_stream=PASS teardown=PASS NO_MATRIX_INIT NO_SWITCH_PROOF
```

335 objects повторно использованы с SHA256: 331 original dependency object и
четыре новых service objects E571. Заново собраны probe и engine_callbacks.
В ASan/UBSan инструментированы эти шесть новых TUs; 331 original dependency TU
не инструментирован. Большие ранее пройденные game suites не повторялись.
SDL dummy не подтверждает слышимое аудио. Populated GUI/texture branches,
ARM64/QEMU и Windows execution этого нового full16 probe — **NOT RUN**.

## SDK evidence

`matrixgame-rangers-host-link-probe.elf`: **ELF64 AArch64 PIE**, 44312168 bytes,
SHA256 `c08af9a7517022a9fc6ca70652a9951775b08038fd718dfcf18e0ecc386451af`.

Архив original host dependencies: 116573416 bytes,
SHA256 `354bb31783f6d6d61d94c6a38329facf2640903528a07adf5ad1fd88b30c4b67`.

`nm -C` подтверждает отсутствие strong undefined и присутствие всех 16 original
Robot functions, MakeRangersHostCallbacks, EngineCallbacks, AudioSession,
MusicSession, TSoundControl_Create и TMusicControl_Create. Link map подтверждает
реальные archive members; старый program.o отсутствует, package reader один.
`CGame::Init` в ELF отсутствует. SDK executable не запускался и не упаковывался.

Артефакты: `build/matrixgame/e2e572-rangers-host-sdk/sdk/`.
Evidence: `reports/matrixgame-e2e572-rangers-host-sdk-evidence.json`.

## Воспроизведение

Из корня checkout с сохранёнными предыдущими dependency objects и SDK:

```powershell
python tools/matrixgame_rangers_host_sdk.py
python tools/matrixgame_rangers_host_checks.py host
python tools/matrixgame_rangers_host_checks.py asan
python tools/matrixgame_rangers_host_audit.py --output reports/matrixgame-e2e572-recheck.json
```

Это проверяемая инкрементальная сборка из текущего локального состояния, не
bootstrap из пустого checkout. SDK recipe требует исходные perf515 flags,
source map, objects/depfiles и реальные OKGF archives. Host recipe требует
E571 проверенные objects и WSL toolchain. Саму готовую цель можно пересобрать
обычным `cmake --build build/matrixgame/e2e572-rangers-host-sdk/sdk --target
matrixgame-rangers-host-link-probe`.

## Init и следующая работа

Предыдущие native CFile/output-folder, StoredStateBackend и scoped copied
service-table binding сохранены. E572 закрывает full SDK dependency link
сервисов; это ещё отдельная программа, а не композиция с NativeInitRuntime.
Последняя аппаратная граница по logs2 остаётся ReadParams → ApplyGammaRamp,
до map constructor. Host full16 proof её не продвигает.

Необходимы общий package/configuration/language/mod owner, согласование CaseConv
с общим configuration scope, композиция EngineCallbacks и real services вокруг
original Init/Deinit, успешная native map/Init-цепочка. После этого можно
собрать обновлённую отдельную deko3d diagnostic NRO обычной существующей целью.
Hardware graphics/audio/HID/focus/F9/recovery остаются пользовательской проверкой.

Аудит подтвердил clean pins upstream cpp/matrixgame/okgf, protected frozen
artifacts, реальные проверяемые ресурсы, старые SDK/NRO outputs и основную NRO.
Последняя отдельная NRO E568 сохранена с SHA256
`d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750`.
Сейвы, игровые ресурсы и upstream не изменялись. Commit/push не выполнялись.
