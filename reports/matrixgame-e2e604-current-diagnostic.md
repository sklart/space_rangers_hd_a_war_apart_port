# E2E-6.04 — fresh diagnostic SDK package и normal-return regression

Стандартные отдельные цели `matrixgame-native-interactive-nro` и
`matrixgame-native-diagnostic-nro` собраны в новом каталоге
`build/matrixgame/e2e604-current-diagnostic/1`. Configure/build завершились
exit0; оба AArch64 ELF не имеют strong undefined symbols. Настоящие original
CGame::Init/Deinit/Draw и RangersHostSession сохранены в symbol table.
198 фактических link inputs записаны с SHA-256. Встроенные пять font assets,
NRO header/segments, ASET/icon/NACP проверены; внешняя RomFS не требуется.

Обе NRO: 14907589 bytes.

- Interactive: `dadfc97e080604479bb3b71fcf9537212332a625798216e03ffb634743694cd2`.
- Quick: `3509f79247f0489efe4d49b229f8b29c9911815f92db383470418aa54d472525`.

Используется E603 original SDK archive с исправленной exception recovery.
Неиспользуемый Gov/Robot код может быть удалён сборщиком мусора линкера:
standalone diagnostic entry не начинает campaign/Gov/FRun. Это сборка для
ручного native Init/Draw/input/audio/cleanup теста, а не native Galaxy proof.
Runtime/HID/GPU image/audio/device recovery на Switch остаются HARDWARE_PENDING.
[Инструкция и точные границы](../docs/matrixgame-e2e604-current-diagnostic.md).

Основная NRO, root Switch-E2E5-TEST и обе E590 NRO сохранили SHA. Protected
game/frozen inputs повторно проверены; игровые ресурсы не копировались и
не изменялись. Пакет не содержит сохранений. Commit/push и действий на устройстве
не было. Production-generated MatrixGame sources byte-identical E601 SDK inputs.

## Настоящий обычный возврат с overlays E603

`windows-difficulty3-economy-campaign0-winter1_1e-heavy-squad/1` — PASS:
full original Init/Draw/combat, natural defeat2, map_time808000/ticks32155,
actual statistics, reward+90, PlanetBattleHistory, snapshot/Gov return,
gamma SDK readback и полная cleanup, exit0. Это новая проверка обычного пути
после exception overlays E603; старое ограничение normal-return regression
для этих overlays закрыто. Windows instrumentation — partial MSVC ASan.

В fixture добавлено обычное выделение не более девяти роботов через настоящие
CMatrixGroup selection/CreateGroupFromCurrent/SelGroupToLogicGroup, с проверкой
фактического group id каждого выбранного робота. Пополнения объединяются
не чаще раза в 30 игровых секунд, одинаковые активные orders не переиздаются.
Постройки по-прежнему через RemoteBuild с точным списанием четырёх ресурсов.

`windows-difficulty3-economy-campaign0-winter1_1e-squad/1` (stock) — INCOMPLETE,
не PASS боя: 120000 тактов/map_time3015125 без естественного исхода; оставалось
6 player robots и 5 hostile robots. 275 оплаченных построек. Исключение лимита
обработано с original snapshot load, сообщением GOV_FRUN_FAILURE и code12;
журнал закрыт. Этот запуск не считается победой, поражением или successful
result return. Ресурсы/HP/AI/result не подменялись. Естественная победа пока
не подтверждена. Следующий основной программный gate — integrated native/deko
caller/presentation/return; отдельная диагностическая NRO не заменяет его.

Архив передачи: `dist/matrixgame-e2e604-diagnostic-TEST-r1.zip`, 10140456 bytes,
SHA-256 `3c6332447084b827b7ecf078bb2b7a5eb40618fc7f9b67a319355e9c779a6af7`.
ZIP testzip и побайтовая проверка всех записей PASS; внутри две NRO, инструкция,
SDK result, build inputs, manifest и SHA256SUMS. Игровых ресурсов/сохранений нет.
[Аудит normal-return regression](matrixgame-e2e604-normal-return-evidence.json).
