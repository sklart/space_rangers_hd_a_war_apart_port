# E2E-5.96 — реальные предварительные объекты Galaxy

Статус общей цели: **PARTIAL**. Настоящие Hangar/TCCInterface/TGalaxy constructor/
destructor проверены; новая кампания и Player ещё не генерировались. Это
зависимость исходной генерации, без доказательства возврата результата боя.
[Команды, результаты и SHA256](matrixgame-e2e596-galaxy-prerequisites-evidence.json).

Добавлен `RangersGalaxyPrerequisites`. При живых data/cache/pixel/script owners
он создаёт оригинальный `TfHangar`, загружает `ML.Hangar` настоящего Main.dat,
выполняет оригинальные `InitializeFromConfig` и `InitializeLayout`, создаёт
оригинальный `TCCInterface` с его lock/Buffer/snapshot chain. Публикует
HangarScreen, RegisteredScreens[screenHangar] и CCInterface только после
завершения обязательных операций. Никакие конструкторы не заменены пустыми
records; алгоритмы snapshot и проверки целостности не менялись.

Owner восстанавливает прежние UiStyleConfig/GameDataConfig/UiDepthConfig,
включая исходно заимствованные правильные корни. Он отвергает чужие корни,
уже занятые Hangar/CCInterface/экран и живую Galaxy. Все Galaxy-потребители
должны завершаться до prerequisite owner. При ошибке config/layout созданное
дерево освобождается оригинальными деструкторами без публикации глобальных
объектов; это проверено на отдельной in-memory копии конфигурации.

В двух циклах на Windows, host и ARM64 выполнены:

- настоящий ангар из 341 control с проверкой MessageLoop identities,
  реальных callbacks и девяти ImageBuffer/ImageControl;
- настоящий TCCInterface и доступ к его snapshot через оригинальный метод;
- неизменённый `TGalaxy_Create`: все исходные module-size/integrity вызовы,
  очистка девяти ShipId, реальный Buffer->Clear, создание коллекций и
  связывание SharedScriptVariables;
- неизменённый `TGalaxy_Destroy`, включая оригинальный CCInterface reset,
  очистку requests и собственных коллекций;
- отсутствие Player до генерации, неизменный MemorySnapshotActive=false;
- отказ без host/script/pixel owner, при чужом CCInterface/корнях,
  повторном owner и живой Galaxy; rollback частичного layout;
- оригинальный cleanup и восстановление холодных/заимствованных корней,
  финализация Script host и data host без загрузки/записи сохранений.

Пустые Stars/Rangers после `TGalaxy_Create` — исходное состояние конструктора,
а не подставной мир. Проба не использует его для боя или имитации Player.
В stdout и итоговом evidence явно указано `campaign_Player=NOT_GENERATED`.

| Проверка | Результат |
| --- | --- |
| Windows partial ASan | PASS, exit 0, два цикла |
| Host ASan/UBSan, leak detection включён | PASS, exit 0, два цикла |
| ARM64/QEMU | PASS, exit 0, два цикла |
| Standard devkitA64 library target | compile PASS |
| Retained API AArch64 ELF | link PASS, 0 strong undefined |
| Полная генерация/Player/Gov OnOpen | NOT RUN |
| Switch | NOT RUN |

Sanitizer instrumentation охватывает новые owner/probe TUs, а не весь
повторно использованный корпус оригинальных зависимостей. Большие ранее
пройденные бои и генерация повторно не запускались. Windows attempt 1
остановился на отсутствующем include определения TObjectList в тесте;
добавлен `types/aMyFunction.hpp`, attempt 2 PASS. Все попытки, логи и
артефакты размещены в отдельных каталогах и сохранены.

Стандартная build-цель: `matrixgame-rangers-galaxy-prerequisites`, публично
зависит от `matrixgame-rangers-script`. SDK ELF удерживает её constructor/
destructor/Hangar API и реальный Script owner, использует проверенные engine
objects E592 и обычные SDK-библиотеки. Это compile/link proof, не запуск.

| Артефакт внутри `build/matrixgame/e2e596-galaxy-prerequisites` | Байты | SHA256 |
| --- | ---: | --- |
| sdk/rangers/libmatrixgame-rangers-galaxy-prerequisites.a | 182366 | 81983209a2bf3440edf23a63f23357f59b49ec00b0186b1d45a6d7cfe79774a0 |
| sdk/matrixgame-galaxy-prerequisites-link.elf | 56979784 | 2231a1cf1ff2c09122874c914aee09fc5166bc0e3a9c638de977669778fed334 |

Воспроизведение с проверенными E595 runtime и E592 SDK inputs:

```text
python tools/matrixgame_galaxy_prerequisites_checks.py windows-asan
python tools/matrixgame_galaxy_prerequisites_checks.py asan
python tools/matrixgame_galaxy_prerequisites_checks.py arm64
python tools/matrixgame_galaxy_prerequisites_sdk.py
python tools/matrixgame_galaxy_prerequisites_audit.py --output reports/matrixgame-e2e596-galaxy-prerequisites-evidence-new.json
```

Достигнутая цепочка: реальные host data/cache → Script VM/builtins/path/two
workers → Hangar(config/layout)/TCCInterface → оригинальный TGalaxy constructor
и destructor. Отдельно ранее проверены Government config/layout и настоящий
FRun до поражения; end-to-end связка с ними ещё не выполнена.

Следующий обязательный участок — исходная инициализация gameplay definitions,
SE ship templates и ScriptTemplates. Они находятся в
`Globals::InitializeGlobalUiRuntime`, в том числе блоки под ReloadScriptTemplates
и `aConst::InitializeGameplayConfig`. Нужно сохранить их исходные операции и
владение, затем выполнить оригинальный `TThreadCreateNewGame_Execute` до
завершения полной кампании/Player/Star/Planet. После этого остаются оригинальный
Gov OnOpen с результатом/наградами/историей/возвратом, естественная победа и
native/deko FRun runtime. Пустые world records и обход обязательных шагов не
используются.

Аудит подтвердил pins и чистоту upstream, SHA256 protected game/frozen inputs,
primary/root NRO, пакетов E590 и SDK artifacts E592. Новая NRO не создавалась,
основная и прежние диагностические NRO сохранены. `D:/logs2.7z` отсутствует,
его текущий hash gate остаётся NOT VERIFIED. Игровые ресурсы и сохранения
не изменялись; commit/push и взаимодействия со Switch не было.
