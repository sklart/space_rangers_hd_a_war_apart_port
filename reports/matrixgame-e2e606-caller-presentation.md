# E2E-6.06 — настоящий caller lease для FRun

Реализована передача одного окна между галактическим UI и оригинальным
MatrixGame, с восстановлением реального состояния устройства и gamma.
Это дальнейшая программная интеграция native/deko; полного Switch Gov context
и аппаратного исполнения на этом этапе нет.

## Цепочка владения

`RangersCallerPresentation` владеет реальным caller window attachment.
`Robot::FRun` через scoped provider захватывает device lease, gamma всех
реально объявленных swapchains и курсор. Окно остаётся присоединено во время
подготовки и loading progress. Непосредственно перед RobotInterface::Run
EnterBattle освобождает caller attachment, чтобы CGame::Init мог присоединить
то же окно обычным способом. После Run/Deinit Restore возвращает caller
attachment, presentation/device state, gamma с readback-проверкой и курсор.

При C++ исключении E603 всё ещё восстанавливает мир через настоящий memory
snapshot и повторно бросает исходную ошибку. Новый lease выполняет реальный
restore при unwinding; ошибка cleanup записывается, а не заменяется PASS.
Владелец отвергает второй активный capture и уничтожение при живом lease.
Повторный Restore идемпотентен только после уже выполненного восстановления.

На Switch native NativeRangersInterface владеет caller phase и canvas E605.
Используются настоящие SwitchWindow/DekoDevice. Native Run требует активного
FRun handoff и освобождённого caller attachment; устаревший caller archive
или прямой вызов Run с ещё присоединённым окном явно отвергается до Init.
State разрушает callback/Run/canvas owners до caller window owner.

Native ветвь FRun использует device/presentation lease вместо D3D9 GR_DXReset.
Захваченная реальная gamma восстанавливается через backend, без фиктивных
COM pointers. Windows SetWindowText/ShowCursor остаются у legacy/reference
окна; native окно возвращает собственное cursor state. Оригинальное GI cursor
state и viewport invalidation сохраняются. Без scoped provider сохраняется
прежняя D3D9 ветвь восстановления.

Production overlay `matrixgame_campaign_caller_overrides.py` включён после
exception overlay в generated_source, а source-cache fingerprint учитывает
новый helper. Upstream не редактировался.

## Выполненные проверки

[Машинный аудит](matrixgame-e2e606-caller-presentation-evidence.json) PASS.

`windows/2/result.json` — PASS, exit0, partial MSVC ASan. Настоящие HWND,
Win32 attachment/WndProc и D3D9 HAL использованы как reference для общего
caller lease. Выполнены original campaign/Gov/snapshot/FRun, full Init/Draw,
затем типизированное исключение из Pump. Во время MatrixGame проверены
BattleActive=true и WindowOwned=false; после исключения — WindowOwned=true,
BattleActive=false. Исходный тип ошибки, реальные 1024×768 target/viewport,
gamma SDK readback, отсутствие лишних device/timer leases, неизменные
money/history, snapshot restore и полная cleanup прошли.

`windows/1` сохраняет отрицательный первый результат (code15): исходная
фикстура создавала backbuffer640×480, а final oracle ожидал1024×768, как после
старого GR_DXReset. Lease правильно возвращал фактически захваченную baseline.
Во второй попытке начальный настоящий D3D9 backbuffer согласован с UI1024×768;
строгие final readback assertions не ослаблялись. Унаследованная stdout строка
`readback 800x600` историческая; фактические SDK assertions проверяют1024×768.

`windows-difficulty3-economy-campaign0-winter1_1e-heavy-squad-caller-lease/1`
в E602 build tree — PASS, exit0. Настоящий бой дошёл до natural defeat2,
map_time808000/ticks32155. Caller window возвращён, gamma/target/viewport
проверены; original Gov обработал actual statistics, reward+90, history,
snapshot/player/planet restore, requested return и cleanup. Исход не
подставлялся; UI autosave handler не вызывался. Это проверка обычного пути
нового lease; победа по-прежнему NOT PROVEN.

`sdk/2/result.json` — PASS, production-generated Robot.cpp и новые native
interface/caller units скомпилированы devkitA64. Отдельный original archive
сохраняет378 members и меняет только Robot.o относительно E605. Отдельная
копия interface archive обновляет interface owner и добавляет caller object,
сохраняя support object. Retained native interface/canvas/caller/campaign ELF
успешно слинкован, AArch64, strong undefined symbols отсутствуют.
ELF57291224 bytes, SHA-256
`dcbd118133dbef7fbe4255585a9889e3a9a719c4c1a7c1d111042d53dde67d98`.
SDK attempt1 предшествует дополнительной проверке native handoff в Run;
attempt2 проверяет финальную версию. GPU runtime НЕ выполнялся.

Для трёх финальных gates сохранены отдельные `as-built-sources/` и
`source-snapshot.json`: байты источников сверены с hashes соответствующих
неизменённых result.json. Это новые evidence files в build directories,
не изменение upstream или существующих frozen-комплектов.

## Воспроизведение и оставшаяся работа

```text
python tools/matrixgame_caller_presentation_windows.py
python tools/matrixgame_caller_presentation_sdk.py
python tools/matrixgame_campaign_frun_windows.py --difficulty 3 --strategy economy --campaign-difficulty 0 --map winter1_1e.cmap --design heavy --formation squad --caller-lease
python tools/matrixgame_caller_presentation_audit.py --output reports/matrixgame-e2e606-caller-presentation-evidence-new.json
```

Runners создают новые attempt directories. Future standard diagnostic builder
берёт последний успешный E606 dependency manifest; E604 NRO/ZIP не обновлялись.
Их binaries, 198 прежних link inputs, основная/root/E590 NRO, protected
game/frozen inputs и чистые upstream pins повторно проверены.

Следующий основной шаг — настоящий native caller context: original CFG/TCC,
software renderer, UI/script/campaign/Gov и общий Window/DekoDevice, с native
loop и возвратом результата. Требуются native UI input/letterbox coordinates,
проверка оставшихся платформенных вызовов полного caller и отдельная NRO с
этой точкой входа. Standalone E604 entry её не заменяет. Native GPU/window/HID
поведение, ошибки восстановления и аппаратный acceptance остаются NOT RUN;
текущий Windows reference не выдаётся за Switch execution.

Игровые ресурсы/сохранения, upstream/frozen и прежние NRO сохранены.
Commit/push и взаимодействия со Switch не было.
