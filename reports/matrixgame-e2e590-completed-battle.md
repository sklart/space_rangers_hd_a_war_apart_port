# E2E-5.90: полный бой до поражения; граница возврата в Galaxy

**PARTIAL по всей задаче.** Настоящий бой на исходной карте дважды дошёл до
поражения, исходной статистики и выхода с кодом 2. Передача шести полей в
настоящий `Robot::RobotBattleStatistics` проверена. `Robot::FRun` и галактический
экран правительства **не выполнялись**: текущий standalone host не создаёт
их обязательный контекст. Победа и Switch runtime остаются NOT RUN.
[Машинный evidence](matrixgame-e2e590-completed-battle-evidence.json).

## Полный бой

Использованы прежние genuine Rangers services/data/audio/music, original
CGame::Init, карта desert1_3e, original form/L3GRun и штатные player команды.
После настоящих startup/input/menu/camera действий каждый из шести существующих
player robot groups получил original `PGOrderAutoAttack`. Внутренние позиции,
HP, оружие, ресурсы, статусы сторон и exit state тестом не задавались.

Между кадрами выполнялось до 200 дополнительных настоящих form Takt(25),
с лимитом 120000 ticks. Это ускоренная симуляция, без realtime/FPS доказательства.
Поиск пути, производство NPC, столкновения, стрельба, урон, уничтожение роботов,
турелей и потеря последней базы выполнялись исходной игрой.

Оба успешных прогона:

| Измерение | Результат |
| --- | --- |
| Natural outcome dialog | `Loose`, исходный code 2 |
| Игровое время диалога | 1707100 мс |
| Дополнительные original form Takt(25) | 67940 |
| Original loop Draw / Pump | 347 / 348 |
| Robots destroyed / turrets destroyed | 11 / 5 |
| Robots built / turrets built / buildings destroyed | 0 / 0 / 0 |
| Исходный signed STAT_TIME | 31607 и 41602 мс |
| Original result → Robot global statistics | Все 6 полей совпали |
| Focus release/recreate, 4 readbacks, Deinit, host cleanup | PASS в каждом прогоне |

STAT_TIME вычисляется оригиналом через timeGetTime, а не deterministic map
clock, поэтому различается между прогонами. После естественного Loose тест
проверяет MMFLAG_WIN=false, нажимает Enter через EngineSink, проверяет
g_ExitState=2, вызывает original Takt для статистики и нажимает Enter ещё раз.
Исходные handlers сами устанавливают exit flag. Далее настоящий SaveResult,
ReadEngineStatistics/WriteStatistics и ReadStatistics сверяют player-side
snapshot с настоящим Rangers global result. CGame/host cleanup остаётся реальным.

Тест не вызывает прямой Damage, не уничтожает объекты искусственно, не применяет
Surrender, FullAuto/demo flags или подставной результат. Нулевая численность
player robots сама по себе не считалась поражением: исходная база ещё жила.
Бой продолжался до исходного диалога проигрыша.

## Два воспроизведённых дефекта

### Усечение callback cookie огнемёта

Первый длинный бой упал с c0000005. CDB + linker map локализовали цепочку
`CFlamePuff::Takt → FindObjects → FlameEnum → WeaponHit`. Pointer-width m_User
копировался в промежуточный **32-битный FlameData::uvalue**, после чего WeaponHit
читал по адресу с потерянными старшими битами.

В `tools/matrixgame_effects_overrides.py` поле port-generated MatrixEffectFlame.cpp
заменено на uintptr_t. Других изменений original generated algorithms нет.
Upstream/header layout owner не менялись. Test-only helper в той же TU вызывает
настоящий static FlameEnum с настоящим live world robot и указателем выше 4 ГБ;
исходный callback доставляет полный адрес. Оба завершённых боя прошли этот oracle.

Baseline executable, source, failed result/runtime log, CDB executable/map/log
сохранены в `build/matrixgame/e2e590-completed-battle/` и `debug/`.
Полная исправленная TU также скомпилирована host ASan/UBSan и ARM64; эти два
delta gates **compile-only**, без утверждения о runtime всего боя под sanitizers.

### Отказ от целого исходного Sound.Flame

После исправления указателя длинный бой обнаружил Pascal exception `WAVE format`.
Read-only наблюдатель original cache loader установил ключ **Sound.Flame**,
parser error **truncated RIFF** и сохранил исходные байты только в private output.
SHA256: `56baed668848829f3f971c697946a2a90532fab057608630f961e1bc4e93d5a8`.

Файл имеет 24748 bytes. RIFF size ошибочно равен 24748 вместо 24740, однако
полный PCM data chunk присутствует: offset 44, 24704 bytes, mono 8-bit 11025 Hz.
Исходный loader такое представление читал; строгий портовый parser отвергал его.

В `e2e4_wav.hpp` добавлен явный `RiffSizePolicy::LegacyTotalBytes`, включённый
только на original Rangers LoadFromConfigBuffer call site через source overlay.
Он допускает точное совпадение RIFF size с оставшейся полной длиной объекта,
не обрезает произвольное превышение и сохраняет все fmt/chunk/frame/bounds
проверки. Default Strict не изменён. Игровой ресурс не исправлялся и не заменялся.

Windows, host ASan/UBSan и ARM64/QEMU выполнили oracle на сохранённом настоящем
ресурсе, проверили exact PCM, prefix offsets, strict rejection, повреждённые
размеры/format и усечения; прежний strict malformed corpus тоже PASS.
Это parser runtime proof, не sanitizer proof полного Init/боя.

Для SDK отдельно скомпилирован настоящий EC_CacheSound.cpp. Создан новый archive
с единственным заменённым member EC_CacheSound.o; SHA256 остальных 377 members
совпали. Прежний archive и его manifest сохранены. Новый manifest/архив переданы
стандартным NRO targets, чтобы SDK не использовал старый precompiled strict
loader. Без этого обновления одного MatrixGame rebuild было бы недостаточно.

Наблюдатели/diagnostic copies читают исходные состояния и не обходят обязательные
операции. Неуспешные compile/AV/WAV попытки сохранены; единый include root устранён
в fixture после initial duplicate pragma-once header definitions. Ошибки чтения
submodule metadata в sandbox решены разрешённым чтением через escalation;
upstream не изменялся.

## Почему Galaxy return пока не PASS

Отдельный executable создал настоящий RangersHostSession и измерил:

```
galaxy=0 player=0 government_screen=0 Robot_Run_interface=0 Rangers_D3D_device=0 callbacks=1
```

Host даёт реальные data/audio/music/callback services и корректно освобождается,
но является standalone owner. Он сознательно требует отсутствия renderer cache/
device. Это не готовая galactic game session.

Исходный `Robot::FRun` до вызова battle Run использует registered screen cursor,
InstallConfig, UI progress/cache и renderer; после возврата восстанавливает
DX/gamma/cursor и инвалидирует настоящий экран. `TfGov::OnOpen` вызывает реальные
MainPanel/PlanetPanel/LoadPanel hooks и обращается к player/current planet,
а затем применяет награды/отношения/history/events в ветках результата 2/3.
Запись Robot global statistics не исполняет эти обязательные операции.

Остаётся программная интеграция borrowed game services/native Run dispatch с
реальной Galaxy/Player/government GI-tree и renderer/session lifecycle. Для её
проверки нужна настоящая новая in-memory game session без SaveLoad; подставные
экран/игрок/обработчик и пропуск UI/economy операций не применялись. Это граница
текущей реализации, а не отсутствие Switch. Цель остаётся активной.

## Воспроизведение

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_completed_battle_checks.py
python tools/matrixgame_legacy_wav_checks.py windows
python tools/matrixgame_legacy_wav_checks.py host
python tools/matrixgame_legacy_wav_checks.py arm64
python tools/matrixgame_flame_cookie_cross_compile.py
python tools/matrixgame_flame_cookie_sdk.py
python tools/matrixgame_completed_battle_audit.py --output reports/matrixgame-e2e590-completed-battle-recheck.json
```

SDK archive впервые подготовлен `matrixgame_legacy_wav_sdk_dependency.py`;
этот builder создаёт файл exclusively и не перезаписывает готовый архив.
Повторная NRO сборка использует уже проверенный dependency result. Galaxy
context-check тоже имеет сохранённый exclusive result; его повторять для
обычного artifact audit не требуется. Полные команды и fingerprints в evidence.

## Проверенные отдельные SDK NRO

Обе стандартные цели собраны AArch64/libnx/deko3d с zero strong undefined,
проверенными NACP/icon/пятью font payloads и новым WAV dependency archive.
Source-family build ID: `82034333146d-80decd88f8f50b99f4af`.

| Артефакт | Bytes | SHA256 |
| --- | --- | --- |
| `build/matrixgame/e2e590-completed-battle/sdk/interactive-nro/matrixgame-native-interactive.nro` | 14899397 | `9a9ee42b529272cc09b5458f7186218d5430aae78acee447643bf7e5cf325e32` |
| `build/matrixgame/e2e590-completed-battle/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro` | 14899397 | `ba7e57f5d322c72c34ab2399ee6d29eecae86456e9e70592034645e079786e72` |

[Инструкция E590](../docs/matrixgame-e2e590-completed-battle.md).
Обе NRO NOT RUN; PC HAL бой не является native/deko hardware proof. Последняя
Switch boundary по logs2 остаётся ReadParams/ApplyGammaRamp до map constructor.

Финальный audit проверил compiled sources/objects/link/binary/outputs, оба
исходных результата, восемь PNG с независимым CRC/zlib/filter decoding,
WAV runtime profiles, complete Flame TU delta compiles, ровно один изменённый
SDK archive member и ровно один изменённый generated MatrixGame source.
Current SDK own-source fingerprints, metadata, packages/fonts, protected
game/frozen fingerprints, обе E589/основная/cumulative NRO и logs2.7z сохранены;
upstream pins чистые. Сохранения не загружались и не записывались.
Commit/push и взаимодействие со Switch не выполнялись.
