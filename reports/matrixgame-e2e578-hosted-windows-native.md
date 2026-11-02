# E2E-5.78 — настоящий hosted native Init prefix под Windows

Статус: **Windows x64 native archive build, joint link, real CGame::Init prefix,
два rollback/reentry cycles — PASS**. Это CPU/lifetime proof; GPU и полный
Init/map не выполнялись. Switch не использовался, новые NRO не выпускались.

Evidence: `reports/matrixgame-e2e578-hosted-windows-native-evidence.json`.
Все команды, logs и промежуточные результаты находятся в
`build/matrixgame/e2e578-hosted-windows-native/`.

## Реальная композиция

Старая native Windows Init infrastructure в основном x86. Проверенный Rangers
host E577 — MSVC x64/LLP64, поэтому старые x86 libraries с ним не смешивались.
Стандартные production CMake targets заново собраны в отдельном x64 каталоге:

| Target | Реально скомпилированных objects |
| --- | ---: |
| matrixgame-native-cache | 124 |
| matrixgame-native-core | 30 |
| matrixgame-native-vector-object | 2 |
| matrixgame-native-navigation | 1 |
| matrixgame-material-combiner | 2 |
| Всего | **159** |

CMake export содержит 283 planned compile commands, включая 124 units других,
не запрошенных executable/audit targets. Они **не** считаются собранными.
Аудит проверяет existence и SHA256 ровно 159 actual objects выбранных пяти
библиотек. Начальное упоминание 283 как compile-count уточнено этим подсчётом.

Release configuration использует `/MT /O2 /UNDEBUG`, без перекрывающего
`/DNDEBUG`. Первоначальная compile-only сборка имела стандартный Release
`/DNDEBUG`; рецепт исправлен и эти пять библиотек пересобраны. В финальном
compile_commands assertions проверены для каждого из 159 consumers.

Original generated MatrixGame/MatrixLib sources получены обычным текущим port
generator из clean pinned upstream. Не добавлены игровые или backend stubs.
Реальные PNG/JPEG/zlib и официальная Microsoft D3DX9 x64 import library взяты
read-only из существующих проверенных зависимостей; их hashes включены в
evidence. Новых сетевых или системных установок не было.

Joint EXE использует **337 неизменённых E577 Rangers objects**, новый prefix
entry и пять новых MatrixGame libraries. Исключены прежний host `main` и
дублирующий package object: тот же реальный shared package.cpp предоставлен
native-core. Mandatory package IO не подменялся. Исходный probe успешно
скомпилировался с обеими наборами headers; отдельная ABI-заглушка не понадобилась.

## Что выполнено

Использован тот же `tests/probe_matrixgame_hosted_native_prefix.cpp`, который
ранее исполнялся на host/ARM64. Изменений его assertions и алгоритма не было.
Device fixture честно выбрасывает `Unsupported("HOSTED_NATIVE_CAPS_BOUNDARY")`
при ReadCaps, не возвращая выдуманных GPU caps или успеха renderer operations.

В каждом из двух циклов:

1. Создан реальный RangersHostSession: config/packages/data, cache/audio/music
   и full16 callback host с настоящим SDL dummy backend.
2. NativeInitRuntime передаёт управление настоящему `CGame::Init`.
3. Original static init, Matrix heap, CFile package open, original data/config
   и cache реально созданы. Диагностика перед cleanup подтверждает
   `statics=1 heap=1 data=1 cache=1`.
4. На ReadCaps original progress callback, AFT raster/render/free и sound
   create/volume/play выполняются через **скопированную** таблицу сервисов.
5. Явная caps-refusal запускает штатный failure path и original cleanup.
6. Matrix globals/cache/output scope и window attachment очищены. Rangers host
   остаётся живым: config/cache identities сохранены, sound ещё доступен через
   его таблицу и затем уничтожается владельцем.
7. Повторный Deinit безопасен; после разрушения host освобождены EngineCallbacks,
   data/cache/audio/music globals и package collection. Следующий цикл проходит.

Итоговый stdout:

```text
HOSTED_NATIVE original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS NO_GPU NO_FULL_INIT NO_SWITCH
```

EXE: `build/matrixgame/e2e578-hosted-windows-native/prefix/hosted-native-prefix.exe`.
SHA256: `bc42f8c8c519df2eb6b9e26f3786df671b18a07c8d3ad600d92f211f0c904923`.
Его existence/build не подменяют приведённый фактический runtime result.

## Входы, вывод и воспроизведение

Для Matrix read root использована отдельная private копия оригинального
`DATA/robots.pkg` и двух upstream text CFG (`data.txt`, `iface.txt`). Это обычные
файлы внутри нового build subtree; Linux symlink прежнего ARM fixture не
использован под Windows, hardlinks к оригинальным resources не создавались.
Source/copy hashes совпали. Rangers game root остался исходным read-only tree.
Private output/user root находится в `prefix/runtime`, Matrix output — в
`native-output` внутри него; оригинальные saves/settings не используются.

Из корня проекта, с Visual Studio C++, Python и существующими зависимостями:

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_hosted_windows_native_build.py
python tools/matrixgame_hosted_windows_native_checks.py
python tools/matrixgame_hosted_windows_native_audit.py --output reports/matrixgame-e2e578-hosted-windows-native-recheck.json
```

`--compile-only` у checks проверяет только probe TU и не заявляет runtime PASS.
Build/check attempts пишутся отдельными результатами и logs; audit создаёт
новый файл exclusively. Runtime не повторялся ради финального read-only audit.
Большие E577 hosted service/AFT suites, старые ARM64/QEMU и ASan suites не
повторялись: текущий gate впервые исполняет joint native prefix на Windows.

Проверены hashes всех link inputs, original/generated sources, 337 reused host
objects, 159 native objects, fixture sources, 78 защищённых game inputs,
доступных frozen artifacts и saves. MatrixGame/cpp/okgf upstream чистые и на
прежних pins. Старые E568/E575 NRO, primary/cumulative NRO и пользовательский
`D:/logs2.7z` сохранены. Commit/push и вмешательства в устройство не было.

## Точная достигнутая цепочка и оставшаяся работа

Windows теперь доказывает ту же original Init/callback/rollback composition,
что host E574 и ARM64 E576. Full16 hosted services отдельно прошли Windows
E577. Native output-folder/CFile и StoredStateBackend уже программно подключены
в прежних E527/E529 и присутствуют в текущей standard SDK composition.

Этот CPU probe останавливается **до успешной device negotiation**. Он не
доказывает проход ReadParams/ApplyGammaRamp, map ctor/StoreState, RobotPreload,
map storage/PrepareMap, interface/final resources и первые map frames в новом
joint native entry. На Windows HAL/GPU этот probe не запускался.

Последняя hardware boundary из сохранённых logs2 остаётся
**ReadParams → ApplyGammaRamp, до map constructor**. Другая, ранняя CPU boundary
этого теста не заменяет и не продвигает аппаратную границу. Конкретный новый
exception после неё не измерен; отсутствующий failure нельзя объявить исправленным.

Следующая работа — дальнейшая интеграция и проверка настоящих map/renderer
dependencies по текущему CGame::Init, без фиктивных обязательных операций.
Для combined SDK entry ещё нужны фактический полный Init, карта/четыре frames,
graphics/readback, audio/HID/focus/F9/recovery. Hardware часть выполняет пользователь.

Сохранённая отдельная diagnostic NRO E575:
`build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`,
SHA256 `9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356`.
Обычная SDK build-цель и ресурсный manifest описаны в
[инструкции E575](../docs/matrixgame-e2e575-hosted-diagnostic.md).
Новой NRO в E578 нет; полный native Init/map остаётся **PARTIAL / NOT RUN**.
