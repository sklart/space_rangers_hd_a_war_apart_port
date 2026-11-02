# E2E-5.71 — настоящий Rangers Music.Robot и владение потоками

Статус: **host PASS, ASan/UBSan новых пяти TUs PASS, SDK archive compile PASS**.
Полный SDK link новых Rangers services и их подключение к native Init остаются
открытыми. Новая NRO на этом этапе не собиралась; аппаратного запуска нет.

## Изменения

`RangersMusicSession` владеет настоящим `TMusicControl`, связывает borrowed
`Main.dat` с `GR_Main::MainDataConfig` и требует живых sound/cache/data owners.
Пустая playlist, выключенная музыка, отсутствующие audio services и чужие
глобальные привязки отклоняются. Таблица callbacks остаётся полной: используются
оригинальные `RobotGetMusicVolume`, `RobotSetMusicVolume`, `RobotPlayMusic`.

Исходный выбор файла вызывает `LowerCaseWideString`, зависящий от
`GR_Main::WideCaseTable`. Первый успешный запуск без этой зависимости выдавал
`Music\planet\bunker.dat`. Owner теперь получает настоящий языковой config,
проверяет `CaseConv` и заполняет таблицу по алгоритму оригинального `GR_Main`.
Чужая непустая таблица не заменяется. Конфигурация остаётся доступной до
завершения workers; таблица освобождается после original music destructor.
Текущая проверка использует явно выбранный `CFG\Rus\Lang.dat`; общий выбор языка
и объединение модов ещё не подключены к native entry.

`RangersAudioSession` теперь допускает штатный режим effects off / music on.
Original sound backend создаётся при любом из двух включённых флагов. Если
backend сбросил запрошенный флаг при ошибке, owner сообщает ошибку и выполняет
cleanup. Исходные флаги восстанавливаются при завершении scope.

Порядок владения: package collection → реальные config/cache trees → audio/cache
owner → music owner → bridge/controllers. Разрушение выполняется в обратном
порядке. Original music destructor останавливает и присоединяет control/current/
queued workers перед освобождением SoundManager и GlobalCache.

## Проверки и их пределы

В обычном host и ASan/UBSan профилях выполнены оба режима: effects+music и
music-only. Во всех четырёх запусках реальный `Music.Robot` выбрал
`music\planet\bunker.dat`; проверены принадлежность оригинальной playlist и
нормализация регистра. Настоящий Vorbis stream потреблён SDL dummy:
1024 played frames, callback count > 0, rejected writes = 0. Проверены
Get/SetMusicVolume, обновление volume streaming buffer и original guard против
повторного запуска уже выбранной музыки. После разрушения owner нет активных
буферов, music/config/cache/sound bindings и SDL audio subsystem.

Также исполнены настоящие original `RobotSetProgress` при отсутствии главного
Rangers D3D device и `RobotReleaseTextures` при пустом TextureCaches. Это штатные
standalone ветки оригинала. Отображение прогресса GUI и освобождение заполненных
GPU texture caches этим не доказаны. Слышимого аудиовыхода нет: SDL dummy.

331 original dependency object повторно использован с проверкой SHA256;
полный большой original-game suite не повторялся. ASan/UBSan инструментирует
только пять новых TUs, включая probe, bridge, factory и owners. Старые original
objects не инструментированы. ARM64/QEMU и Windows reference для этого нового
музыкального этапа **NOT RUN**; предыдущие результаты других этапов не подменяют
эти gates.

SDK archive содержит compiled music owner и producer. SHA256
`libmatrixgame-engine-callbacks.a`:
`679681904b167a52edb352270d8b790edb30dbdaf2884724dd883604522424bc`
(446882 bytes). Existing bridge link probe проходит без strong undefined,
но новый music/audio host в нём не достигается: это **не** полный SDK host link.

## Воспроизведение

Из корня проекта установленным Python выполнить:

```powershell
python tools/matrixgame_rangers_music_checks.py host
python tools/matrixgame_rangers_music_checks.py asan
python tools/matrixgame_rangers_music_sdk.py
python tools/matrixgame_rangers_music_audit.py --output reports/matrixgame-e2e571-recheck.json
```

Host требует существующих проверенных E569/E570 dependency objects, WSL g++,
SDL2/Vorbis и original game resources. Link использует отдельный Linux `/tmp`
staging для скорости и сохраняет его путь. Выходы текущей проверки находятся в
`build/matrixgame/e2e571-rangers-music/{host-final,asan-final,sdk-final}`.
Первые результаты до исправления CaseConv сохранены в соседних host/asan/sdk.
Evidence: `reports/matrixgame-e2e571-rangers-music-evidence.json`.

## Достигнутая Init-цепочка и оставшиеся компоненты

Ранее завершены private minimap/output-folder provider с original CFile,
StoredStateBackend, native device/settings/state и scoped full16 service binding
через original rollback/Deinit. E569/E570/E571 исполнили реальные sound, AFT text
и music services отдельно на host. Это ещё не единый successful native CGame::Init.

Последняя аппаратная граница по logs2 остаётся `ReadParams → ApplyGammaRamp`,
до map constructor. Последующие исправления и зависимости имеют программные
проверки, но новую аппаратную границу не устанавливают.

Следующие необходимые работы:

- Полная devkitA64 компоновка оригинальных Rangers host dependencies с новым
  producer/owners вместо проверки лишь archive и старого bridge.
- Общий owner загрузки configuration/packages/language/mods с временем жизни
  до original Deinit; согласование его CaseConv с текущим music scope.
- Композиция AudioSession/MusicSession/EngineCallbacks с NativeInitRuntime и
  полной copied SMGDRangersInterface без частичных/фиктивных callbacks.
- Успешная единая native Init/map-цепочка; populated GUI/texture branches при
  появлении соответствующего настоящего renderer context.
- Отдельная обновлённая diagnostic NRO после полного link, затем пользовательская
  проверка Switch graphics/input/focus/F9/audio/recovery.

Последняя отдельная NRO E568 сохранена, SHA256
`d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750`.
Аудит подтвердил оба clean upstream pins, проверяемые frozen-артефакты,
ресурсы и primary/cumulative NRO. Игровые данные и сейвы не изменялись.
Commit/push и взаимодействие с устройством не выполнялись.
