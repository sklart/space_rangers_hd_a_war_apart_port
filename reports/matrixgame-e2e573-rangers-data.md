# E2E-5.73 — общий Rangers data/package owner и DAT cleanup

Статус: **original config differential PASS; borrowed full16 host services PASS;
ASan/UBSan новых TUs PASS; full SDK link PASS**. Единая native CGame::Init/map
цепочка ещё не запущена. Новая NRO не выпускалась; Switch не использовался.

## Общий владелец данных

`RangersDataSession` в `port/matrixgame/native/` владеет оригинальными package
collection/lock, install/language install trees, списками install blocks модов,
MainDataConfig/LanguageDataConfig/CacheDataRoot и WideCaseTable. Он использует
настоящие original loaders, `aPacket::LoadConfiguredPackages` и `MergeFrom`.
Заглушек для операций загрузки, звука, текста или музыки не добавлено.

Сохранён порядок оригинального GR_Main: loose provider → language-mod packages
→ language packages → mod packages → base packages; Main load/merges → language
load/merges → CacheData load/merges → CaseConv. Для пустого набора модов, как в
оригинальном do/while, посещается `Mods\` с пустым именем. Поддерживаются выбранный
язык, fallback на русский, исходный ModCFG и явный skip-mods режим.

Временные деревья принадлежат RAII owners. При ошибке освобождаются опубликованные
roots, mod lists/trees, packages/lock; прежние RequestedLanguage/SelectedLanguage,
SelectedMods/display suffix и SkipModsOnReload восстанавливаются. Чужие глобальные
owners отклоняются до загрузки. Уже созданные audio/music services должны быть
разрушены до data owner; нарушение lifetime contract завершает процесс.

Caller заранее выбирает существующие ec_file game и private-user roots. Сам data
owner не загружает пользовательские settings и не пишет config dumps, ресурсы или
сейвы. `RangersDataOptions` задаёт язык, ModCFG path и skip-mods; по умолчанию
используется исходный `Mods\ModCFG.txt`. Общая композиция этих roots с native
MatrixGame package/provider binding ещё предстоит.

AudioSession/MusicSession получили явные конструкторы от `RangersDataSession&`.
Они заимствуют его CacheData/Main/Language/CaseConv и освобождают только свои
cache/audio/music resources. Legacy конструкторы для отдельно загруженных trees
сохранены; чужие глобальные привязки ими не заменяются. После разрушения borrowed
audio/music Main/CacheData/CaseConv остаются доступными до data owner destructor.

## Найденная утечка и исправление исходного loader

Первый ASan запуск `language-error` выявил **76 bytes в двух allocations**:
`TBlockParEC::LoadFromEncryptedDatFile` создаёт TFileEC, но ошибка открытия выходит
до обычного `pas::free(FileObj)`. В TDataEC имеется аналогичная схема владения.
Очищение только внешних config globals не устраняет эту утечку.

В `tools/e2e_dat_cleanup.py` добавлен узкий generated-source overlay обоих методов:
local resource scope освобождает FileObj/Buf при исключениях; прежние явные точки
освобождения сохранены и обнуляют pointers. Работа чтения, проверки целостности,
декодирование и построение trees остаются исходными. Overlay включён в обычный
`e2e_source_overrides.generated_source`; upstream не редактировался.

Для текущих проверок две прежние effective TUs преобразованы в отдельные port
копии и пересобраны, включая ASan/UBSan. Новый SDK archive содержит эти два objects
вместо прежних и 376 неизменённых объектов E572. Никакая старая archive не изменена.

Baseline leak log сохранён в
`build/matrixgame/e2e573-rangers-data/asan/run-language-error.log`.
Отдельный sanitizer regression `dat-cleanup-asan` вызывает оба loader при
отсутствующем файле и при первом чтении однобайтного private fixture:

```text
DAT_CLEANUP missing_open=2 short_read=2 exception=PASS handles_closed=PASS NO_FORMAT_CHANGES
```

ASan/UBSan с detect_leaks=1/haltonerror проходят; original exceptions не подавлены.
После ошибок все 16 loose-file slots свободны; trees остаются пустыми.

## Дифференциальные и lifetime проверки

В обычном host и ASan/UBSan профилях выполнено пять exact comparisons со штатными
`GR_Main::LoadLanguageAndPackages` и `LoadDatConfigAndModOverrides`:

1. Пустой requested language → русский.
2. Явный английский.
3. Неустановленный requested language → fallback на русский.
4. Настоящий установленный `Tweaks\SR2LoadingScreen`.
5. Тот же выбор с skip-mods.

Original `WriteTextBuffer`/`WriteToBlock` сериализуют Main/Language/CacheData в
private test outputs; сравниваются полные bytes и state/package order. Все
совпадают с reference и сохранённым original baseline до исправления DAT cleanup.
Для реального мода Main и CacheData действительно отличаются от baseline;
при skip-mods все три trees снова совпадают с базовыми.

Ошибки отсутствующего package и отсутствующего Lang.dat подтверждают constructor
rollback, восстановление прежнего состояния и последующую успешную загрузку
английского в том же процессе. Duplicate owner не разрушает действующий контекст.

`probe_matrixgame_rangers_owned_host.cpp` создаёт data → audio/music → EngineCallbacks
и выполняет все 16 MatrixGame callbacks с original sound/AFT/music resources и
SDL dummy. Дополнительно host-only invalid SDL driver проверяет, что отказ
borrowed AudioSession сохраняет Main/CacheData/CaseConv; после смены на dummy
реальный audio backend запускается. По завершении callbacks/music/audio data
context остаётся жив, затем data destructor очищает globals.

Итог обоих профилей:

```text
OWNED_HOST_ENGINE full16=PASS text=PASS sound=PASS music_stream=PASS teardown=PASS NO_MATRIX_INIT NO_SWITCH_PROOF
```

В каждом финальном host binary ASan/UBSan инструментирует девять TUs, включая оба
original DAT-loader и новые service/adapter TUs. 329 original dependency TUs
остаются неинструментированными. Большие game suites не повторялись.
Слышимого звука/GUI/GPU нет. ARM64/QEMU и Windows execution этих новых probes
**NOT RUN**; ранее пройденные другие этапы не подменяют эти проверки.

## SDK и воспроизведение

Полная отдельная цель `matrixgame-rangers-host-link-probe` собрана с новым data
owner и borrowed constructors. В ELF реально присутствуют DataSession,
AudioSession/MusicSession(RangersDataSession&), EngineCallbacks и все 16 original
Robot functions; strong undefined = 0. Это ELF64 AArch64 PIE, SDK ELF **NOT RUN**.
Он не содержит CGame::Init и не является диагностической deko3d NRO.

ELF: 44373856 bytes,
SHA256 `298f6c15154e40f47eca41e158c61b09d06b89d5a02a96cb3d03dd0ede924131`.
Callbacks archive: 798906 bytes,
SHA256 `2eb6d099383066c5d71806e5336c386bd4716b08f86c0bec75b66472d601f8c3`.

Финальные outputs:
`build/matrixgame/e2e573-rangers-data/{host-final,asan-final,sdk-final,dat-cleanup-asan}`.
Ранние runs в host/asan/sdk сохранены. Evidence:
`reports/matrixgame-e2e573-rangers-data-evidence.json`.

```powershell
python tools/matrixgame_rangers_data_checks.py host
python tools/matrixgame_rangers_data_checks.py asan
python tools/matrixgame_rangers_data_sdk.py
python tools/matrixgame_dat_cleanup_checks.py
python tools/matrixgame_rangers_data_audit.py --output reports/matrixgame-e2e573-recheck.json
```

Как E572, это инкрементальный рецепт из сохранённого локального состояния:
требуются прежние проверенные host/SDK dependency objects, SDK/WSL toolchains и
исходная установленная игра. Bootstrap из пустого checkout этим не реализован.

## Init-цепочка и оставшиеся gates

Data/package loading, реальные host services и их общий lifetime теперь имеют
отдельные runtime/differential/SDK доказательства. Native CFile/output-folder,
StoredStateBackend и copied full16 binding E568 сохранены. Следующий шаг —
композиция DataSession/AudioSession/MusicSession/EngineCallbacks с NativeInitRuntime
через original rollback/Deinit, включая согласованную видимость packages и режимы
отключённого звука/музыки. Единая native Init/map-цепочка ещё открыта.

После этой композиции нужна обновлённая отдельная diagnostic NRO на libnx/deko3d.
Последняя отдельная NRO E568 сохранена, SHA256
`d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750`.
Аппаратная граница logs2 остаётся ReadParams → ApplyGammaRamp, до map constructor.
Switch image/audio/HID/focus/F9/recovery потребуют пользовательского запуска.

Аудит подтвердил clean pins cpp/matrixgame/okgf, frozen/primary/cumulative NRO и
неизменность проверяемых DATA/CFG/install/Mods файлов настоящей игры. Игровые
ресурсы, сейвы и upstream не менялись; commit/push и устройство не использовались.
