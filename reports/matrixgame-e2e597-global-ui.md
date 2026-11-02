# E2E-5.97 — полный original global UI/gameplay runtime

Общая цель остаётся **PARTIAL**: исходный global initializer подключён целиком,
а полная кампания/Player и возврат результата боя ещё не выполнены.
[Итоговый evidence: команды, результаты, SHA256 и protected-input audit](matrixgame-e2e597-global-ui-evidence.json).

Добавлен production owner `RangersGlobalUiSession`, вызывающий настоящий
`Globals::InitializeGlobalUiRuntime`. Вместе с UI исполняются исходные
инициализации script conditions/global variables, race/pirate/dominator ship
templates, planet templates, film/process resources, gameplay configuration,
weapon visual resources, greetings/adverts и robot-map definitions. Блоки
инициализации не вырезались и не заменялись копиями алгоритмов.

Проверка на настоящих Main.dat/Lang.dat/CFG.TXT получила в каждом цикле:

- 36 экранов, 10826 controls с правильными MessageLoop identities;
- 40 скомпилированных script templates и 32 глобальные переменные;
- 207 planet templates, gameplay GalaxyStarCount=73;
- настоящие cache-loader и save-writer workers, PrimaryFilm/SecondaryFilm,
  SpaceObjectUiLoop, SpaceProcess и AchievementsScreen.

Save writer только создан и затем завершён исходным деструктором. Задания
записи, SaveGameToFile и загрузка сохранений не вызывались. User settings
прочитаны из настоящего game CFG.TXT; необязательный newgame block начинается
пустым, как в оригинальном GR_Main при отсутствии newgame.txt. Игра,
конфигурация и ресурсы на диске не изменяются; вывод изолирован в build.

Owner требует живые data/cache/script/pixel/scratch/settings и оригинальные
Main.dat ML/Data/ZPos roots. Чужие registered или unregistered screens,
worker/process/film/popup/render-template и ship-template owners отвергаются
до вызова инициализации. Прежние экраны нельзя подменять во время scope.
Galaxy и её потребители должны завершаться раньше этого owner.

Финализация вызывает исходные `FinalizeGlobalUiRuntime` и
`ResetScriptHostRuntimeState`, а также настоящие деструкторы двух объектов,
пропущенных оригинальным Finalize: AchievementsScreen и SpaceObjectUiLoop.
После cleanup templates пусты, GlobalScriptVariables снова содержит два
исходных значения, ReloadScriptTemplates восстановлен, workers и UI globals
освобождены. Проверены два повторных цикла и отказ повторному owner.

Аварийный откат проверен после настоящего исключения StrToInt на неверном
`ML.Gov.Border` в отдельной in-memory копии конфигурации. К этому месту уже
созданы templates и часть экранов. После освобождения частично созданного
runtime следуют два успешных полных цикла на неизменённых игровых данных.

Сохранён и отдельный отрицательный baseline Windows attempt 3: удаление
обязательных узлов `ML.Load.Panel` приводило к исходному null dereference
в `TfLoad::InitializeLayout`, а не C++ exception с unwind. MAP локализовал
вызов; оригинальный Layout не менялся. Это известная граница повреждённой
конфигурации, **не PASS безопасной обработки отсутствующих UI nodes**.
Основные проверки используют подлинную конфигурацию; тест rollback использует
настоящее исключение парсера. Attempt 1 сохранил compile error в тестовом
имени файла; после исправления типа имени файла обычные циклы attempt 2 PASS.

Windows partial ASan и host ASan/UBSan с leak detection: два полных цикла,
откат и cleanup PASS, exit0. Новые owner/probe TUs инструментированы;
оригинальные зависимости используются из ранее проверенного корпуса и
не имеют полной instrumentation. Большие пройденные бои не повторялись.

ARM64/QEMU attempt 1: compile/link PASS, partial rollback и цикл 0 PASS,
общий runtime limit 180 секунд достигнут во время цикла 1. Это **не полный
PASS**. Для завершения gate предусмотрен отдельный runtime-only runner с
неизменённым готовым бинарником и лимитом 480 секунд; исходная попытка и её
stdout сохранены. Runtime-only attempt 2 завершился с кодом 0: rollback и
оба полных цикла PASS; binary/object hashes совпадают с attempt 1. CPU время
было существенно меньше wall time, в ожидании процесса наблюдался файловый
WSL wait channel `p9_client_rpc`. Это не аппаратная проверка Switch.

Стандартная SDK target `matrixgame-rangers-global-ui` и retained API AArch64
ELF compile/link PASS, 0 strong undefined; runtime NOT RUN. В ELF удерживаются
constructor/destructor нового owner и реальный Script owner. Прежние E592
engine objects и библиотеки повторно использованы.

| Артефакт внутри `build/matrixgame/e2e597-global-ui` | Байты | SHA256 |
| --- | ---: | --- |
| sdk/rangers/libmatrixgame-rangers-global-ui.a | 116548 | 0b7cc472a4e4861cc6bcd2fec6c5e984882535fab2faaedc4d60dc8ec30c813f |
| sdk/matrixgame-global-ui-link.elf | 62309600 | b7618ead85f9cbd606b4a72678f4e81581e88819930ad5a631f15e1735600de2 |

Воспроизведение с проверенными E595 runtime и E592 SDK dependencies:

```text
python tools/matrixgame_global_ui_checks.py windows-asan
python tools/matrixgame_global_ui_checks.py asan
python tools/matrixgame_global_ui_checks.py arm64
python tools/matrixgame_global_ui_sdk.py
python tools/matrixgame_global_ui_audit.py --output reports/matrixgame-e2e597-global-ui-evidence-new.json
```

При сохранённой ARM64 attempt 1 с TIMEOUT и отсутствии старого живого процесса:

```text
python tools/matrixgame_global_ui_arm64_runtime.py
```

Этот runner не компилирует и не линкует повторно; проверяет неизменность
binary/object/dependency/source hashes и требует два завершённых цикла и exit0.
Он добавлен после SDK metadata; его отдельный hash включается в runtime evidence.

Следующий реальный участок: `TThreadCreateNewGame_Execute` с настоящим worker
и полным warmup, затем Player/Star/Planet → Gov OnOpen → бой → награды,
история и возврат экрана. Перед запуском необходимо исправить LP64 allocation:
исходный fGameSettings выделяет 12 bytes для PStorageEntry, а packed запись
`TStorageEntry` из двух указателей и int32 занимает 20 bytes на AArch64.
Это подтверждено отдельной devkitA64 compile-only static_assert пробой
`fixture/storage_entry_abi.cpp`; upstream не менялся. Нужна адресная port
overlay правка с настоящим sizeof, а не изменение игровых данных или
пропуск стартовых предметов. Это известная следующая зависимость, не
утверждение об отсутствии других LP64 проблем генератора.

Точный compile-only ABI command и SHA256 сохранены в
`build/matrixgame/e2e597-global-ui/fixture/storage_entry_abi.json`.
Итоговый audit подтвердил сохранность game/frozen fingerprints, pinned и
чистых upstream, primary/root NRO, обоих пакетов E590 и SDK artifacts E592.
Внешний `D:/logs2.7z` отсутствует: его текущий hash gate NOT VERIFIED.

Естественная победа, original Gov result/reward/history/return и native/deko
FRun runtime остаются открытыми. Новая NRO не создавалась, прежние NRO и
сохранения не менялись; commit/push и взаимодействия со Switch не было.
