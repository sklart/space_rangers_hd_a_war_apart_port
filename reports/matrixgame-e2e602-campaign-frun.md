# E2E-6.02 — настоящий бой из Gov и возврат результата

**Подтверждено естественное поражение с записью результата в настоящую
сгенерированную галактику на Windows reference. Победа ещё не подтверждена.**

Проверяемая цепочка: real Host/Script/GlobalUI/Campaign → original приказы
взлёта/посадки → реальная планета → Gov layout/OnOpen → original memory snapshot
→ Robot::FRun → NativeRangersInterface с настоящим Windows D3D9 HAL → полный
CGame::Init и исходные Takt/Draw/combat → natural loss dialog → statistics/input
exit → исходные restore/Gov result processing → reward/history/dialog/
RequestedScreenId=Government → original world/UI/script/host cleanup.

Исход/флаги победы, HP, армии, ресурсы, возвращённая статистика и история тестом
не задаются. Входные параметры native entry — настоящий map definition id1
`Desert1_3e.cmap` и штатный уровень поддержки. Галактика создаётся original RNG,
без host generation seed override. MatrixGame получает прежний fixture seed37.

Граница входа — Gov::OnOpen с действительными map/difficulty/transition inputs.
StartPlanetBattle* UI handlers вызывают auto-save, поэтому при запрете изменения
сохранений они не запускаются. Внутри проверяемой цепочки обязательные snapshot,
FRun и result operations выполняются полностью. Не доказан полный main Galaxy
UI Run loop: проверяются настоящие OnOpen/OnClose/MessageLoopStack и запрос
перехода; loop сам не заменяется заглушкой. Файловые сохранения игры не вызываются.

| Gate | Фактический результат |
| --- | --- |
| windows/4 | partial MSVC ASan PASS, exit0; difficulty1, natural state2, map_time668275, ticks26594; reward +170; original dialog590 units |
| windows-difficulty3/1 | partial MSVC ASan PASS, exit0; difficulty3, natural state2, map_time427300, ticks17002; reward +30; original dialog589 units |
| windows-difficulty3-economy/2 | partial MSVC ASan PASS; natural state2, map_time1251475, ticks49805; 114 original RemoteBuild команд с точным списанием ресурсов, reward +30 |
| windows-difficulty3-economy-campaign0/1 | partial MSVC ASan PASS; штатная campaign difficulty0, natural state2, map_time575125, ticks22886, reward +90 |
| Victory | NOT PROVEN; после чтения настоящего каталога запущен сценарий Winter1_1e.cmap, id12, с одним противником |
| Native/deko FRun | NOT RUN |
| Switch | NOT RUN |

Все шесть полей RobotBattleStatistics сравниваются с original CGame::SaveResult,
затем с добавленной PlanetBattleHistory entry. Проверены map id, difficulty,
completion mode, turn, LastPlanetBattleTurn, прирост денег, Player/Planet identity
после пересоздания мира, итоговый диалог и RequestedScreenId. Full Reset через
реальный SDK и gamma readback PASS; device/timer leases пусты, CGame globals
очищены. Статистика не выдумана caller-ом.

Original RobotSetProgress действительно рисует Gov в настоящем RGB565 canvas.
Использован существующий renderer_platform InitializeSoftwareRenderer с реальными
buffers/pixel format/OKGF callbacks и original shutdown. Ранее CPU-only Caller
владел только pixel/scratch; для этого сценария он теперь заимствует графические
ресурсы у действительного renderer owner. Проверены presentations>0 и ненулевой
framebuffer hash. Host implementation не доказывает физический показ Gov UI;
MatrixGame form Draw и Windows D3D9 HAL проверяются отдельно реальным SDK.

Отрицательные попытки сохранены:

* windows/1 — literal filename lookup не нашёл map definition.
* windows/2 — path basename comparison ещё учитывал регистр; native definition
  использует `Desert1_3e.cmap`. Исправлен только fixture selector, исходный путь
  целиком передаётся original FRun.
* windows/3 — ASan nil ScreenRenderBuffer при original RobotSetProgress/Present.
  Исправлен caller ownership/context, progress или рисование не отключались.
* windows-difficulty3-economy/1 — compile-only обращение к private field;
  fixture исправлен на штатный GetRobotsCnt. Следующая попытка выполняется.

Первая economy fixture использует original PGOrderAutoDefence/PGOrderAutoCapture и
CConstructor::RemoteBuild, реальную blueprint configuration, capacity/robot
limits и affordability. После каждой постройки проверяется точное исходное
списание цены. Ресурсы/юниты не добавляются тестом и исход боя не выставляется.
Следующая версия проверяет приказы всех групп каждые четыре command polls,
независимо от числа построек, чтобы новые группы не оставались без приказов
во время ожидания ресурсов. Уже действующий mpo_AutoCapture не переиздаётся:
оригинальный PGOrderAutoCapture очищает путь и passive orders даже при том же
режиме, поэтому старый fixture мог мешать завершению собственного захвата.
Штатная campaign difficulty передаётся через
RangersCampaignOptions; ни HP, ни enemy budgets тестом не меняются.

`map-catalog/1/result.json` PASS: отдельное чтение настоящих 74 definitions через
original Host и InitializeRobotMapDefinitions, без повторной campaign generation.
У Winter1_1e.cmap id12, MinWins1/MaxWins10. Fixture выбирает настоящий definition
по filename и передаёт original Map целиком. Training/demo definitions не
подставляются вместо обычного боя. Winter attempt1 выполнила original Init и
Draw, затем сохранила fixture assertion `!groups.empty()`: эта настоящая карта
начинает без готовых player robots. Для economy mode начальный Issue заменён
на реальный CampaignPlayerCommands/RemoteBuild; отсутствие стартовых юнитов
теперь не подменяется их созданием тестом. Winter attempt2 завершилась естественным поражением (state2, map_time1747600, 69552 ticks): original history/statistics/return и cleanup PASS, награда +90. Победа этим запуском не подтверждена.

Воспроизведение (новые attempt directories):

```text
python tools/matrixgame_campaign_frun_windows.py
python tools/matrixgame_campaign_frun_windows.py --difficulty 3
python tools/matrixgame_campaign_frun_windows.py --difficulty 3 --strategy economy
python tools/matrixgame_campaign_frun_windows.py --difficulty 3 --strategy economy --campaign-difficulty 0 --map winter1_1e.cmap
python tools/matrixgame_campaign_map_catalog.py
python tools/matrixgame_campaign_frun_audit.py --output reports/matrixgame-e2e602-campaign-frun-evidence-new.json
```

[Machine-readable defeat-only audit](matrixgame-e2e602-campaign-frun-evidence.json)
проверяет actual binaries/objects, expanded compiler inputs, original Init journal,
natural dialog/outcome, reward/history/return markers, protected inputs и прежние
NRO. Старые shared fixture/tool source hashes являются историческими: добавление
следующего режима не перезаписывает expanded CPP предыдущего успешного запуска.
Не каждый original dependency TU инструментирован; это partial Windows ASan.

Новая NRO на данном этапе не создавалась. Основная NRO, игровые данные,
сохранения, upstream/frozen сохранены; commit/push и действий со Switch не было.
Остаются естественная победа и её original reward/completion path, native/deko
исполнение и обновлённая отдельная диагностическая сборка для ручного теста.

Проверка исходного RobotToLogicGroup выявила ошибку economy fixture: метод
каждый раз выделяет новую группу и сбрасывает Order/road path. Его повторный
вызов для уже назначенного робота прерывал ранее выданные auto-capture приказы.
Старые natural-loss результаты остаются действительными для цепочки
результата/cleanup, но не оценивают работоспособность нормальной стратегии.
Исправление должно сохранять GetGroupLogic при валидном существующем номере
и вызывать исходный RobotToLogicGroup только для ещё не назначенных роботов.

Дополнительная проверка штатного конструктора (`--design heavy`):
оригинальный SuperDjeans (тот же путь, что CIFaceMenu) выбирает TRACK,
FULLSTACK, четыре LASER на настоящих common pylons и DYNAMO. Создание
роботов остаётся только RemoteBuild с точным списанием всех четырёх ресурсов.
`windows-difficulty3-economy-campaign0-winter1_1e-heavy/1` до исправления
группировки: natural defeat, map_time1289950/ticks51337, награда +90.
`...-heavy/2` с сохранением существующих GetGroupLogic: natural defeat,
map_time787750/ticks31349, награда +90. Оба полных прогона exit0 и прошли
возврат, историю, статистику, gamma restore и очистку. Обязательные операции
не заменялись; победа не подставлялась. Тяжёлая конструкция не дала победы.

`windows-difficulty3-economy-campaign0-winter1_1e/3` (stock, corrected
GetGroupLogic): NOT PASS. Оригинальная симуляция достигла 120000 тактов
(map_time3015125; 6 player robots, 8 hostile robots, одна player base),
не показав natural win/loss. Fixture бросает runtime_error на своём лимите;
процесс завершился 0xc0000409, журнал не получил journal_close. Явной
диагностики ASan в runtime.log нет. Нельзя называть это победой, поражением,
успешным возвратом или подтверждённой ошибкой памяти. Исходный fGov::OnOpen
повторно бросает непаскалевское исключение до RestoreGameFromMemorySnapshot;
RangersCampaignSession::Impl запрещает уничтожение с активным snapshot и
вызывает terminate. Это установленная по исходникам возможная цепочка,
точный стек аварийного выхода пока не получен. Требуется проверка исключений
на границе FRun/Gov с настоящим восстановлением мира и ресурсов; обходить
инвариант владельца или подставлять исход нельзя. Все артефакты сохранены.

[Аудит пяти завершённых natural-loss сценариев](matrixgame-e2e602-campaign-frun-strategy-evidence.json)
PASS: два исходных baseline и три дополнительных Winter/heavy запуска.
Stock attempt3 в PASS-аудит не включён. Аудит повторно проверил upstream pins,
защищённые данные/frozen и прежние NRO. Победа/native-deko/new-NRO остаются
открытыми; новая граница — корректное восстановление при C++ исключении.
