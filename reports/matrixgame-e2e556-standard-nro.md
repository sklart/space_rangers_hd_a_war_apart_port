# E2E-5.56 — программная передача MatrixGame и штатная NRO-цель

26.10.2026. **SDK BUILD / NRO FORMAT / SOURCE AUDIT PASS; FULL NATIVE INIT PARTIAL.**

Исходное локальное состояние уже содержало завершённые E527 output provider,
E528 minimap cleanup, E529 stored state и последующие E530–E554. Они не
переписывались и не выдаются за новые изменения этого этапа. Первоначально
проверены Git status/diff и результаты этих этапов. Родительский HEAD:
`82034333146d34b6b7f7a2d429c7261afa4c276f`. Незакоммиченная работа сохранена.

## Что добавлено сейчас

В `port/matrixgame/native-engine/CMakeLists.txt` добавлена отдельная цель
`matrixgame-native-diagnostic-nro`. Она зависит от настоящего diagnostic ELF
и использует установленные SDK `nacptool` и `elf2nro`. Icon взят из libnx.
NACP/NRO создаются исключительно в `<binary-dir>/diagnostic-nro`; цель не
входит в ALL и не затрагивает основную NRO. Опция
`MATRIXGAME_DIAGNOSTIC_NRO=OFF` оставляет возможность собирать только ELF.

Добавлен `tools/matrixgame_standard_nro_audit.py`: проверяет текущие исходники,
generated originals, обязательные символы, отсутствие strong undefined,
структуру NRO, embedded icon/NACP и hashes защищённых входов. Аудитор читает
прежние CPU/Windows результаты, явно записывает изменения исходников после
исторических прогонов и не обозначает их новым исполнением. Выходной JSON
создаётся эксклюзивно: существующий отчёт не перезаписывается.

## Артефакт текущего состояния

- NRO: `build/matrixgame/e2e556-standard-nro/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`
- Размер: **6 936 773 байта**.
- SHA-256: `5e1dcd00e33e8d956fabf9b4f4cc2128be3ccefc00c2bec821c338ceaeeb2de1`.
- ELF SHA-256: `b826a1224e7791ec2f2a7f1f1f61ae352f3c1016200b3d46647b704e51639d09`.
- Embedded build ID: `82034333146d-3ceba50c0e188afa9ce6`.
- [Текущий финальный аудит](matrixgame-e2e556-standard-nro-evidence.json).

`sdk/result.json` сохраняет первый полный compile/link этого этапа. После
добавления аудитора штатный CMake повторно вычислил fingerprint, пересобрал
diagnostic main, перелинковал ELF и обновил NRO. Поэтому **окончательные hashes
и metadata приведены в финальном аудите выше**, а не в первоначальном
`sdk/result.json`. Все 255 generated original files совпадают с E554.
Из прежнего набора source inputs изменился только CMakeLists; добавился аудитор.

## Output provider, CFile и владение

`NativeGameOptions.output_root` копируется в принадлежащий runtime NativeScope.
FolderItem удерживает shared snapshot root; Windows Shell PIDL освобождается
CoTaskMemFree только в настоящей Windows ветке. Native provider создаёт реальные
каталоги, проверяет Unicode, traversal/symlink и не подменяет ошибку успехом.
Оригинальный PathToOutputFiles сохраняет SpaceRangersHD/Cache/Screenshots layout.
Две filesystem-границы minimap используют FilesystemPath; PNG и original draw
не заменены. При Deinit output binding снимается после original map/font/cache
cleanup. Выходное дерево не удаляется.

Тесты E527 вызывают исходный интерфейс CFile Create/Write/OpenRead/Read/Close
через production native file backend; Windows reference проверяет исходную
Windows границу. Cold render/store и warm load настоящего minimap PNG выполнены
в двух сохранённых Windows HAL Init/Draw/Deinit. Native diagnostic использует
отдельный `<unique-run>/output`; resource read scope живёт до завершения
package/cache cleanup. Resource root: `sdmc:/switch/matrixgame-diag/game`,
run journals: `sdmc:/switch/matrixgame-diag/logs`. На Switch эти пути в новой
сборке не исполнялись.

## Проверки и область доказательства

| Требование | Подтверждение и граница |
| --- | --- |
| Output/CFile/cleanup | Сохранённые E527 HOST ASan/UBSan, ARM64/QEMU, Windows filesystem, Windows HAL и Init prefix/rollback PASS. Все восемь result.json проверены. Большие тесты не повторялись. |
| StoredStateBackend | E529 HOST ASan/UBSan, ARM64/QEMU, Windows scalar reference и SDK PASS; Windows HAL material/fixed-vertex comparisons в отдельном evidence. DekoDevice действительно наследует NativeStoredStateBackend; Store/Restore использует authoritative NativeStateStore, 103 render registers, 13 sampler registers ×8, 8 lights и material. Это scalar state, отдельно от owning surface/texture capture lease. |
| Последующие зависимости | В текущих исходниках уже подключены surfaces/readback, minimap offscreen cleanup, fog/stencil/line routes, text, reset/presentation ownership, form/loop, resource read scope, journal и E547–E554 offline fixes. Их адресные доказательства перечислены в текущей Init sequence и отдельных отчётах. |
| Новая SDK-сборка | Полный devkitA64 compile/link PASS, затем штатная NRO-цель PASS. Strong undefined: 0. В ELF присутствуют original CGame::Init, CDeviceState::StoreState, native stored adapter и output scope. Предупреждения deprecated wstring_convert остаются; ошибок компиляции нет. |
| Новый аудит | Текущие source fingerprints и 255 generated original hashes совпали; NRO segments/ASET/icon/NACP проверены, внешнего RomFS нет. |
| Сохранность | Upstream clean на `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`; проверены 15 protected inputs, дополнительные frozen hashes, 107 аппаратных входов, основная и cumulative NRO, оба прежних diagnostic пакета. |
| Устройство | NOT RUN / HARDWARE_PENDING. В этом этапе нет переноса файлов или взаимодействия с Switch. Commit/push не выполнялись. |

Прежние test source hashes местами отличаются из-за уже существующих E528–E554;
полные списки записаны в JSON. Исторический PASS не превращается в новый полный
runtime PASS текущего кода. Перед изменением build plumbing все source inputs
совпали с последней E554 SDK metadata; production алгоритмы этого этапа не менялись.

## Точная цепочка Init и незакрытые границы

Программно связана цепочка: standalone libnx/HID/window → DekoDevice → GPU smoke
→ NativeInitRuntime/resource+output scopes → original CGame::Init → static/heap/
packages/CFG/DAT/cache → platform attachment/caps/state capture → map members/
debug font/cursor → LoadSide/RobotPreload → stor.Load → PrepareMap/terrain/water/
shadows/minimap → ApplyVideoParams → interface/Text::Render → effects/default
resources/objects lights → original form/L3GRun Takt/Draw/Present → original
Deinit и restore владельцев. Развёрнутая последовательность и порядок cleanup:
[matrixgame-init-sequence.md](../docs/matrixgame-init-sequence.md).

Сохранённый полный Windows Init/PrepareMap/Draw не доказывает native GPU Init.
Последняя историческая аппаратная граница run2 — attachment и BC1 upload,
затем ошибочный отказ пустого applet poll до draw/Present/original Init.
Исправления E547–E554 входят в новую NRO, но аппаратно не подтверждены.
Первый возможный отказ внутри полного native Init пока не наблюдался.

Остаются непроверенными на Switch: devoptab filesystem semantics, настоящие
allocation/upload/readback и memory budget карты, offscreen/default-resource
lifetime, полный PrepareMap, первый map Draw/Present, input/focus/resume,
reset/queue-loss recovery и длительный бой. Raw Windows HAL state-block retry
FAIL E534 остаётся отдельным открытым контрактом; он не подменяется CPU
rollback PASS. F12 screenshot и borrowed-host GDI transition capture явно
Unsupported; это не обязательные операции standalone первого Init. Встраивание
в основную игру, аппаратная приёмка и прежние независимые GI/P0 вопросы не закрыты.

## Воспроизведение из текущего локального дерева

Нужны установленные devkitPro/devkitA64/libnx/deko3d, Windows CMake/Ninja/Python
и уже проверенные original font assets в `build/matrixgame/e2e521/font/windows`
и `build/matrixgame/e2e524-text/windows`. Игровые ресурсы берутся из личной
локальной копии; не входят в NRO и этим этапом не копируются.

```powershell
$env:DEVKITPRO = 'C:/devkitPro'
$env:DEVKITA64 = 'C:/devkitPro/devkitA64'
$taskPython = 'C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe'
$taskCmake = 'C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
& $taskPython tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e556-repro --diagnostic-source build/matrixgame/e2e543-journal/source
& $taskCmake --build build/matrixgame/e2e556-repro/sdk --target matrixgame-native-diagnostic-nro -j 4
& $taskPython tools/matrixgame_standard_nro_audit.py --build build/matrixgame/e2e556-repro/sdk --output build/matrixgame/e2e556-repro/handoff-audit.json
```

Первая команда сохраняет полный CMake configure command в result.json, включая
Ninja/toolchain/font paths; дальше используется стандартная build-цель. Для
повторного аудита следует выбрать новое имя JSON. Сборка использует установленный
debug deko3d SDK, как предыдущая диагностика. Это воспроизводимость процедуры
для текущего дерева, не заявление о побайтной воспроизводимости разных SDK.
Аппаратный запуск и решение о переносе артефакта остаются за пользователем.
