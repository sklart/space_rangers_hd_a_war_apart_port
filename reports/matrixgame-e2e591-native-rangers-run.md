# E2E-5.91: настоящий типизированный Run-вход Rangers

**PARTIAL по возврату в галактическую часть.** Run slot, совместимый с
`Robot::TRobotRun`, реализован и исполнил три полных боя до естественного
поражения в Windows HAL reference mode. Реальный `Robot::FRun`/`TfGov::OnOpen`
ещё не вызывался: Galaxy/Player/government renderer/UI session отсутствуют.
[Машинный evidence](matrixgame-e2e591-native-rangers-run-evidence.json).

## Что подключено

`port/matrixgame/native-engine/native_rangers_run.hpp/.cpp` добавляет
`NativeRangersRunBinding`. Его `Entry()` возвращает настоящий типизированный
TRobotRun callable, а не исполнение Windows DLL ABI через касты. Есть явный
native/deko context и отдельно помеченный `RangersLegacyReferenceTag` для PC.

Run выполняет следующие обязательные операции:

1. Проверяет наличие scoped binding, отсутствие рекурсивного входа и output storage.
2. Преобразует UTF-16 строки вызывающего в обычный platform wchar_t через
   существующий endian-independent MatrixLib codec. Null остаётся null.
3. Копирует реальные caller settings через ReadSettings/ToEngineSettings либо
   использует собственную копию настроек context. Caller storage не меняется.
4. Вызывает настоящий InitializeNativeGame либо явно выбранный reference
   InitializeOriginalGame, создаёт оригинальный form и запускает L3GRun.
5. Снимает настоящий CGame::SaveResult и исходный exit state.
6. Уничтожает form, выполняет Deinit и проверяет настоящий device restore result.
7. Только после успешного cleanup записывает шесть статистик в переданный
   TPlanetBattleStatistics и возвращает исходный код.

При Init/loop/restore ошибке исключение выходит вызывающему; подставного результата
нет. Output commit происходит после cleanup, поэтому отказ не оставляет частично
переписанных counters. Signed time сохраняется. Победа/поражение не вычисляются
обвязкой, а берутся из исходного состояния игры.

Контекст владеет копиями roots/settings/service table; Window/Device/draw sink и
сам callback host остаются borrowed. Они должны пережить binding и вызов.
Caller также устанавливает настоящий native_loop::Scope. Повторный binding на
том же потоке и рекурсивный Run запрещены; вызов Entry после снятия последнего
binding отвергается. Внутренняя running-защита снимается и при исключении.

32-битные module/window поля старого TRobotRun контракта не преобразуются в
указатели. Реальные полные identities берутся из явно зарегистрированных
providers. Native settings по-прежнему отвергают legacy D3D identities через
PrepareNativeGameSettings; reference mode использует настоящий PC D3D9 provider.

Неполная RobotInterface table **не устанавливается**. Initialize, Finalize,
Support не заменены заглушками или фиктивным успехом. Этот компонент предоставляет
ровно работающий Run slot для последующей настоящей caller integration.

## Windows runtime и владение

Два прогона с null caller settings проверили owned defaults, ещё один — реальный
для FRun случай с nonnull TRobotDisplaySettingsPrefix. Все три исполнили полную
исходную карту с original PGOrderAutoAttack, производством/уроном/потерей базы,
естественным Loose и штатными Enter/statistics/exit handlers.

| Прогон | Код | STAT_TIME | Robot kills | Turret kills | Transition frames | Loop draws | Дополнительные Takt(25) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Owned defaults 1 | 2 | 72377 | 10 | 5 | 30 | 450 | 82528 |
| Owned defaults 2 | 2 | 181870 | 11 | 5 | 30 | 377 | 67940 |
| Nonnull caller settings | 2 | 76727 | 10 | 5 | 30 | 450 | 82528 |

Это ускоренная симуляция с настоящими form Takt, не realtime/FPS доказательство.
Wall-clock-derived STAT_TIME и итоговые показатели между прогонами не объявляются
детерминированными. Каждый результат сравнивается со своим независимым оригинальным
SaveResult snapshot в момент исходного диалога. Все шесть полей совпали после
уничтожения карты, host callbacks/data ещё были живы.

Каждый context проверил:

- null result storage отклоняется до старта;
- настоящий Init на отсутствующей UTF-16 карте (кириллица + surrogate pair в имени)
  завершается исключением, шесть sentinel output fields не меняются;
- исходные globals очищены, и следующий полноценный Run после этого отказа успешен;
- resource root и defaults действительно скопированы: caller меняет
  исходное строковое хранилище и меняет defaults после создания binding;
- у nonnull caller settings все bytes сохранены, включая поля/padding;
- poison legacy tokens 0xffffffff/0xdeadbeef не трактуются как pointers;
- после возвращения form/map/heap/cache globals уже освобождены, а host ещё жив;
- после binding destruction сохранённый Entry отвергает вызов;
- исходный SDK main проверяет gamma/window/target/depth/viewport, device/timer
  leases и итоговое host освобождение. Cwd не меняется.

**MSVC ASan — частичный:** инструментированы новый Run adapter и reference test
TUs (в nonnull profile также SDK main); большие оригинальные archives переиспользованы
без переинструментирования. STL container annotations выключены для совместимости
с этими старшими archives, address/heap/stack проверки не подавлялись. Это не
full-engine ASan/LSan/UBSan proof. Новые readback image oracles здесь не запускались;
E590 image доказательства сохраняют свою прежнюю область.

## Host/ARM64/SDK

Полная production TU адаптера скомпилирована host ASan/UBSan и ARM64.
Оба gates **compile-only**, без runtime claim на этих платформах.

Добавлен opt-in CMake option `MATRIXGAME_BUILD_RANGERS_RUN=ON` и стандартная
library target `matrixgame-native-rangers-run`. Default targets не меняются.
SDK проверка построила её и обычный interactive ELF в отдельном E591 build dir,
затем с теми же настоящими CMake link inputs слинковала отдельный API link ELF,
удерживая все strong Run class methods, включая native и reference constructors.
Все нужные symbols retained, strong undefined = 0, AArch64 header PASS.
Это link proof; новый ELF **не запускался**, новой NRO не создавалось.

| Артефакт | Bytes | SHA256 |
| --- | --- | --- |
| `build/matrixgame/e2e591-native-rangers-run/sdk/libmatrixgame-native-rangers-run.a` | 327214 | `723baec65b54f0a9192ed6e835f29eda9edd842f5e97e3521117e1bf68aa1257` |
| `build/matrixgame/e2e591-native-rangers-run/sdk/matrixgame-native-rangers-run-link.elf` | 53813200 | `3a885bc33caf3314e2d0c13fcdac6c6f737c4eaa6c42f72c39690de65a25e35c` |

ELF включает обычную SDK диагностическую composition и symbols/debug info;
его размер не является оценкой памяти на Switch. Бинарники E590 остаются
актуальными отдельными NRO и сохранены побайтно. Generated original algorithms
совпали с E590. Upstream/frozen/game inputs, основная/cumulative NRO и logs2.7z
сверены и сохранены. Сохранения не загружались и не записывались, commit/push и
взаимодействие со Switch не выполнялись.

## Остаток полной задачи

Run slot теперь реально работает. По-прежнему нужны настоящие native
Initialize/Finalize/Support integration и установка complete Robot interface,
реальная in-memory Galaxy/Player/government GI-tree и renderer/session lifecycle,
вызов исходного FRun и исполнение reward/relation/history/transition веток
TfGov::OnOpen. Локальный caller output не является доказательством этих операций.
Победа и native/deko hardware execution также остаются открытыми.
Цель порта не объявляется завершённой.

## Воспроизведение

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_native_rangers_run_windows.py
python tools/matrixgame_native_rangers_run_settings_windows.py
python tools/matrixgame_native_rangers_run_cross_compile.py
python tools/matrixgame_native_rangers_run_sdk.py
python tools/matrixgame_native_rangers_run_audit.py --output reports/matrixgame-e2e591-native-rangers-run-recheck.json
```

Повторение runtime для обычной проверки artifacts не требуется. Audit создаёт
новый JSON exclusively. Все exact compiler/link/runtime commands и input hashes
записаны в evidence. Стандартная библиотека после configure:
`cmake --build build/matrixgame/e2e591-native-rangers-run/sdk --target matrixgame-native-rangers-run`.
