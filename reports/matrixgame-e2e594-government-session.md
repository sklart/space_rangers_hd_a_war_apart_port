# E2E-5.94: настоящий правительственный GI-экран

**PARTIAL по возврату в Galaxy.** Реализован scoped `RangersGovernmentSession`:
исходные `TfGov_Create → InitializeFromConfig → InitializeLayout → TfGov_Destroy`.
Во всех трёх runtime-профилях созданы 395 настоящих controls, исполнены два
цикла владения и cleanup. `TfGov::OnOpen`, Player/Galaxy, rewards/history и
галактический переход этим этапом не исполнялись.
[Машинный evidence](matrixgame-e2e594-government-session-evidence.json).

## Production интеграция и владение

Владелец заимствует живые MainDataConfig, GlobalCache, CurrentPixelFormat и
renderer/UI context caller. Он связывает оригинальные ML/Data/ZPos roots
из Main.dat как UiStyleConfig/GameDataConfig/UiDepthConfig. Прежние roots
могут быть null либо теми же настоящими блоками; чужие roots отклоняются.
Ни один config/resource file не редактируется.

TfGov конструирует настоящие main/planet/load panels и ScriptDialogNames.
Конфигурация Gov загружается оригинальным GI loader, layout и event callbacks
назначаются исходными методами. Globals::GovernmentScreen и registry slot
screenGovernment публикуются только после успешного layout. Ошибка до этого
освобождает частичное дерево исходными destructors и восстанавливает roots.

Деструктор снимает обе регистрации, уничтожает экран при ещё живых roots,
затем восстанавливает заимствованные указатели. Он не уничтожает внешний
data/cache/pixel/renderer owner. Duplicate government ownership запрещено.
Публикация и cleanup рассчитаны на обычный один игровой поток.

Первый Windows runtime обнаружил отсутствующий UiDepthConfig: именованный
слой читался через null. Сохранены executable, runtime log и map-only relink
в `build/matrixgame/e2e594-government-session/windows-asan`. Stack RVA
локализовал `GetParamOrMarker → SetDepthByName → LoadFromBlock`.
Исправление подключает настоящий Main.dat/ZPos, как обычный GR_Main startup;
новый успешный комплект находится в `windows-asan-depth`.

## Завершённые проверки

| Профиль | Реальное исполнение | Результат |
| --- | --- | --- |
| Windows HAL, partial MSVC ASan | Original Gov config/tree/layout, 395 controls × 2, callbacks, duplicate owner, cleanup; borrowed E593 SDL/SDK caller | PASS, exit 0 |
| Host ASan/UBSan с LSan enabled | Original CPU Gov layout 395 × 2, missing prerequisites, partial-tree failure rollback, cold/borrowed roots, foreign/duplicate ownership, cleanup | PASS, exit 0 |
| ARM64/QEMU | Тот же настоящий CPU lifecycle и negative cases, 395 × 2 | PASS, exit 0 |
| devkitA64 | Стандартная CMake library и отдельный ELF с удержанными API methods | Compile/link PASS; runtime NOT RUN |

CPU negative fixture копирует MainDataConfig в отдельный объект и очищает
только Panel в этой частной копии. Исходный loader создаёт настоящее частичное
дерево; исходный layout отвергает отсутствие обязательного control, а новый
owner освобождает его и восстанавливает globals. Этот fixture не подменяет
успешный Gov: положительные циклы загружают неизменённый настоящий Main.dat.

Проверены оригинальные MainPanel keyboard/UserMsgAdd click bindings, наличие
PM_EndTurn/PM_Ship/TalkPA/PP_Gov и принадлежность дерева экрану. CPU цикл с
предварительно связанными настоящими roots подтверждает их сохранение после
уничтожения owner. Host остаётся жив до завершения всей UI-деструкции.

ASan/UBSan покрывает новые TUs; большие исходные dependencies переиспользованы,
поэтому это **partial sanitizer proof**, а не full-engine instrumentation.
CPU профили используют настоящие muted host services. Windows использует
настоящий E593 SDL foreign wrapper и SDK device; target/viewport/gamma readback
относится к caller restoration, а не к изображению правительственного экрана.
Gov Draw/OnOpen и custom-cursor image restoration не объявляются проверенными.

Большой бой E593 и пройденные большие E590–E592 suites не повторялись.
Для CPU профилей скомпилированы только новые owner/probe TUs, прежние original
objects проверены хешами. В SDK собиралась новая target с её необходимыми
dependencies; прежние MatrixGame engine objects использованы для link closure.

## Артефакты и оставшаяся цепочка

Стандартная target: `matrixgame-rangers-government` в native/CMakeLists.txt.
SDK artifacts и их точные SHA256/размеры записаны в evidence. Отдельный
AArch64 ELF удерживает constructor/destructor/Screen API; сильных undefined
symbols нет. Это не новая NRO и не Switch execution.

| Артефакт | Байты | SHA256 |
| --- | ---: | --- |
| sdk/rangers/libmatrixgame-rangers-government.a | 145492 | 73adcf3b606c8364e8996e0b399a6be99e48e00d89f6c1097253813dc9b14b86 |
| sdk/matrixgame-government-link.elf | 57702832 | 98c0f1f365ed9279f1d44f588022e4707cb2b03e7c943383df826e24fa79baf2 |

Итоговый audit проверяет новые source/header/object/link/binary hashes,
protected game/frozen inputs, primary/root NRO, оба E590 пакета, сохранность
E592 SDK artifacts и pinned upstream status. Предыдущий SDK result/metadata/
artifacts сохранены в sdk/history перед обновлением metadata.

Внешний `D:/logs2.7z` сейчас отсутствует; текущую неизменность архива проверить
нельзя. Его прежний SHA256 сохранён как historical evidence, отдельный gate
имеет статус **NOT VERIFIED; EXTERNAL FILE ABSENT**. Архив не создавался и не
восстанавливался. Проверки кода и доступных защищённых входов завершены
отдельно; archive gate остаётся открытым.

Воспроизведение с установленными toolchains и проверенными E576/E593 inputs:

```text
python tools/matrixgame_government_windows.py
python tools/matrixgame_government_cpu_checks.py asan
python tools/matrixgame_government_cpu_checks.py arm64
python tools/matrixgame_government_sdk.py
python tools/matrixgame_government_audit.py --output reports/matrixgame-e2e594-government-session-evidence-new.json
```

Для настоящего OnOpen остаются как минимум реальные in-memory Galaxy/Player/
Planet и script runtime owners, включая TCCInterface/Buffer, HangarScreen и
скриптовые tables/events/path-node pool. Нужны исходная генерация новой кампании
и привязка player/planet, затем Gov battle request → FRun → OnOpen с настоящими
reward/history/dialog/transition действиями. Эти объекты не заменены пустыми
records или извлечённой копией reward-алгоритма.

Также открыты естественная победа и native/deko FRun boundary; аппаратную
проверку выполняет пользователь. Игровые ресурсы/сохранения и прежние NRO
не изменялись. Commit/push не выполнялись.
