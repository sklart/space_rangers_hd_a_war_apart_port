# E2E-5.8 — стабилизация и границы доказательства

Дата: 2026-10-16. Исходный HEAD `baa7399`, аппаратный baseline `0fdf602`,
удалённый master перед работой `300f0e3`. Все исправления E2E-5.3–5.7 сохранены.
MatrixGame отдельно зафиксирован коммитом `c8e9899`; cpp/okgf не изменены.
Пользовательские saves и игровые ресурсы не изменялись. Дисковое сохранение
в аппаратной конфигурации остаётся выключено.

## Аппаратные исходные данные

Прочитаны предыдущий отчёт 9b691cb, сводка hardware-e959e85-followup и новый
G:/pack.zip. Новый SHA-256 архива:
`5cb257e597186137e61ca4e86ca90b22c37daf459ec887ff5e0922d10ce23862`.
Архив теперь содержит **0fdf602**, а не прежний 9b691cb. Старый отчёт с SHA
`77dd4eae…` сохранён; он описывает другой архив того же имени.
[Метаданные нового прогона](hardware-0fdf602-20261016.json).

Генерация: 300/300, stage 8 complete, total=444320 мс,
`introduction status=continue-enabled`. Позднее — исключение
`EC_FileStream::TFileStreamEC`; музыка theo_running_away.dat и Out of memory.
Наличие continue-enabled — программная публикация, не подтверждение видимой
активности кнопки или завершённого перехода на приставке.

Последняя memory запись: arena=3320578048, used=485119664,
free_blocks=2835458384, top_free=8192, uncommitted=1196032.
Сумма free_blocks и top_free не устанавливает наибольший непрерывный блок.
Пробный захват всего свободного heap не выполнялся. **ROOT CAUSE UNCONFIRMED**.

## Исправления

| Замечание | Причина/исправление | Host | ARM64 | Switch | Оставшийся риск |
|---|---|---|---|---|---|
| UI замер после генерации | В 12 парных участках EC_File исключение backend/строки могло удержать PackageFileLock; добавлен RAII guard с прежними границами обычного пути | real TFileStreamEC ASan/UBSan PASS | syntax PASS | HARDWARE PENDING | Связь дефекта с конкретным freeze ещё требует консоли |
| FileLock при открытии ресурса | EC_Data защищал чтение, но AcquireReadWriteHandle до try; RAII покрывает также открытие | полный переход + OOM PASS | syntax PASS | HARDWARE PENDING | Не все игровые ресурсы намеренно повреждались |
| Потерянные nested handle references | GetSize не делал ReleaseHandle при ошибке; ScopeExit балансирует acquisition | real file/stream PASS | syntax PASS | HARDWARE PENDING | Cleanup рассчитан на не бросающий backend Close |
| Пакетный FILE* при OOM | resize/packed/decode allocation могли покинуть ReadPayload/LoadFolder с открытым FILE; unique ownership закрывает при любом выходе | injected payload/decode/block OOM + fd count PASS | syntax PASS | HARDWARE PENDING | Физическая причина OOM неизвестна |
| Жизненный цикл аварийного worker | ThreadEntry обходил self-close на bad_alloc; владелец теперь единственный закрывает handle после ожидания | fault harness обнаруживал terminate до исправления; после PASS | syntax PASS | HARDWARE PENDING | Производительность/планирование OS thread на Switch не измерены |
| OOM создания frontend handle | Backend event/thread создавался до выделяющего память frontend; frontend резервируется заранее с rollback | sync regression + real stream PASS | syntax PASS | HARDWARE PENDING | Не все глобальные allocator failures модели Win32 инъецированы |
| Загрузка после взлёта | Сохранены PendingLoads/CacheLock/queue fixes E2E-5.7; повторены normal/fault пути | first presentation id=16 + 60 cycles; 4 реальных malloc/realloc отказа PASS | syntax PASS | HARDWARE PENDING | Исходные аппаратные 51% не закрыты |
| Лишние проекции аркадных линий | Кэш 128 результатов внутри одного Update с неизменной матрицей; точные ключи трёх double, исходный Extended projector | 4.8 млн сравнений × host/ARM64 QEMU PASS; real arcade 60 cycles | syntax PASS | HARDWARE TEST REQUIRED | Микропроверка не заменяет сравнение управляемого боя/изображения на консоли |
| Недостаточная детализация генерации | Семплирование star-day/prepare, base ship-day/logic, equipment, bonus каждые 64 вызова; cumulative atomics, без TLS/аллокаций | 300 ходов, метрики получены | syntax PASS | HARDWARE PENDING | Derived AI не равен base ship-logic; нулевой счётчик не означает отсутствие AI |

Порядок блокировок: cache producer отпускает CacheLock перед IO/decode;
EC_Data берёт FileLock → PackageFileLock. Пакетный backend не вызывает музыку
или графический кэш. Музыка берёт DecodeLock → file/package; файловый worker
не требует DecodeLock и не очищает GlobalCache. Поэтому к этим путям не добавлена
обратная связь PackageFileLock → CacheLock/DecodeLock. BufferLock защищает
только копирование/перестановку; чтение и ожидание worker происходят вне него.
RAII completion/IdleEvent и запрет Start на аварийном worker E2E-5.7 сохранены.

## Проверки T1–T6

[Машиночитаемые результаты](e2e58-validation.json) содержат SHA журналов,
идентификаторы изолированных host runs и существенные контрольные точки.

- T1: реальный сгенерированный EC_File + EC_FileStream + EC_Thread,
  платформенные package/ec_file_adapter и настоящие thread/events связаны
  с effective host game. Эти пять модулей и harness инструментированы
  ASan/UBSan; остальные игровые объекты переиспользованы из optimized host.
  Отказ открытия, OOM полного payload, decoder buffer, packed block,
  испорченный последний блок после 16 успешных декодирований; одновременно
  второй читатель. Подтверждены занятое ожидание, последующее освобождение
  общей блокировки, IdleEvent, IsRunning=false, join/close, освобождение
  пакетного FILE* и работоспособный новый reader. Это controlled fixture,
  не аппаратная инъекция и не тест реального музыкального файла.
- Переключение музыки: actual generated getter/control, отказ при выборе
  имени следующего трека, освобождение DecodeLock/ControlLock и повтор PASS.
- T2: actual cache/loader body ASan/UBSan: read/decode bad_alloc, конкурентный
  waiter, retry, event/buffer cleanup; начало/середина очереди и отмена PASS.
  Полный host с настоящими ресурсами: new game → ангар → взлёт → first
  presentation StarMap id=16 `dfce1a09a2c0161a` → 60 циклов, 68.84 с,
  crash=false/timeout=false. Отдельно четыре реальных malloc/realloc отказа:
  AllocEC 1982908, ReAllocREC 12402677, recovery cache 121784400 → 0,
  StarMap `5d3d94063eb0c9c2`, 60 циклов, 71.25 с. Сценарии/seed различаются.
- T3: исходные 300 ходов, 300/300 и continue-enabled. Алгоритмы генерации,
  RNG и порядок вычислений не менялись. Общий world/RNG hash A/B **NOT RUN**:
  новых оптимизаций генерации с требованием такой эквивалентности не принято.
- T4: исходный projector против кэша, 4.8 млн побитных сравнений в четырёх
  режимах округления на каждой архитектуре, signed zero, смена матриц,
  переполнение ёмкости и коллизии. Число projector calls 2193441 host,
  2192550 ARM64: различие распределения связано с результатами входных
  вычислений под разными FP моделями, сравнение внутри архитектуры точное.
  Сохранён steering regression: нейтраль/направления/диагонали/разворот,
  cursor/right stick/touch/fire. Real arcade 60 cycles без crash/timeout.
  Первый запуск помечен timeout из-за ошибки чтения маркера в Python monitor;
  журнал показывает завершённый сценарий, исправленный monitor прошёл 37.48 с.
- T5: Options реально открыт дважды (39.20 с), имя/focus/drag/duplicate-release
  regression PASS. Ангар/диалог покрыты полным сценарием. Дополнительный Intro
  с SkipIntro=False прошёл видео → меню → New Game (39.46 с), без crash/timeout.
  Меню модов обнаружило шесть реальных установленных записей и завершило
  60 кадров (44.50 с). Выполнение модов по-прежнему отложено. Credits/магазины,
  квесты и видимое touchscreen/keyboard поведение в этом этапе отдельно
  не перепройдены; прежняя доказательная база сохранена, новый PASS для них
  не заявляется.
- T6: devkitA64 -fsyntax-only девяти модулей; оба preflight PASS;
  254 imports / 45 dynamic / UNKNOWN=0, семантика классификаций прежняя.
  Pointer-width new_unclassified=0, 25 tool tests PASS.

В WSL потребовался расширенный доступ; исходный E_ACCESSDENIED был ограничением
окружения. Одна host-линковка столкнулась с Text file busy при параллельно
работающем старом ELF; после завершения процесса линковка повторена без
перекомпиляции. Этот неуспешный запуск не записан как PASS.

## Производительность и решения

| Гипотеза | Решение | Основание/ограничение |
|---|---|---|
| RAII для package/file lock | ACCEPT | Воспроизводимая утечка блокировки; controlled fault recovery |
| Повторное использование packed/decoded buffers между блоками | ACCEPT | Те же байты и порядок zlib blocks; real stream и corrupt tests; убраны повторные allocations на каждом блоке |
| Повторное использование проекций общих world-line vertices | ACCEPT для численной эквивалентности; HARDWARE TEST REQUIRED для скорости | 42.62% hits в реальной host аркаде; матрица/ключи ограничены текущим Update |
| Геометрическое отсечение поколений по AABB | REJECT для включения сейчас | IntersectSegmentsF может менять output и имеет tolerance; без полного world/RNG oracle нельзя менять произвольные проверки |
| Приближённая тригонометрия/сужение Extended/fast-math | REJECT | Нарушает заданные ограничения; не включено |
| Ускорение за счёт пропуска ходов/AI/событий | REJECT | 300 ходов и механика сохранены |

Аппаратные baselines: 9b691cb total=426.547 с, stage1=68.523 с,
stage6=349.792 с; 0fdf602 total=444.320 с. Это **не A/B**.
Host нового профиля: stage1=0.510 с, stage6=8.264 с, total=10.083 с;
нельзя сопоставлять с Switch или выдавать за аппаратное ускорение.
В последнем sampled window: star-day 21899 вызовов/343 samples/118517 us,
base ship-day 246184/3847/32286 us, equipment 675942/10562/1800 us,
bonus 26034607/406791/28149 us. Это inclusive sampled durations, не total
времена и не проценты CPU; вложенные интервалы не суммируются. Счётчики
cumulative и сбрасываются только перезапуском процесса. Детальный allocation,
conversion/event/derived-AI профиль пока не выделен. Цель 30% на Switch
**не подтверждена**. Старые round float/double и trig reuse сохранены.

Контрольный старый host ELF также прошёл аркаду (26.68 с). В единственном
доступном окне phase4 среднее было 408 us против 415 us нового host: по этим
разным запускам улучшение всей фазы не доказано. На одинаковых входах microbench
100000 проекций checksum совпал: host 1884 → 1405 us, ARM64 QEMU 308815 →
147204 us. Это локальная оценка устранённых вычислений и накладных расходов;
ни её проценты, ни длительность всего процесса не переносятся на FPS Switch.

## Артефакт, CI и аппаратный gate

Одна incremental combined ARM64 сборка выполнена из чистого коммита
`1e473351a973ee0933fdeec0f53ac9d2f10d2c89`; перекомпилировано 9 объектов,
остальные переиспользованы. ELF64 AArch64 PIE, undefined symbols=0,
build_dirty=false, E2E5_DISABLE_DISK_SAVE=1, -O2 -fno-strict-aliasing.

- NRO: `port/switch/build/e2e-game-combined-experiment/SpaceRangersHDAWarApartE2E4Combined-1e47335.nro`, 16435504 байт;
  SHA-256 `4315D141E1B1E77374CDA11282E14FBA828F89DE4BE2D96099E528D030EA4527`.
- Matching ELF: `SpaceRangersHDAWarApartE2E4Combined-1e47335.elf` в том же каталоге,
  56367808 байт; SHA-256 `F89D0B8DF6717B7315FF37964F5433BE334D46DA7502EE99D2F0B0AC39C28BE7`.
- Build ID `f22bb7db118da4b0d06710699ea40d0afe6afc65` совпадает с NRO module ID.
- [Паспорт сборки](e2e58-artifact-provenance.json) также лежит рядом с NRO;
  отдельные versioned .sha256 созданы. Все 10 прежних versioned NRO/ELF
  сверены и не изменены. Полная карта содержит 298 уникальных effective sources.

Статус CI в паспорте относится к моменту упаковки: **CI NOT RUN at artifact
verification**. Следующий push документации и исходников запускает существующий
package-host workflow; его текущий результат проверяется на GitHub для точного
SHA отправленного коммита. Локальная ARM64-сборка не является CI PASS.

Первый CI [38868245685](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/runs/38868245685)
для `5bb56e4` прошёл e2e5-fast, e2e4-sanitizers и e2e5-arm64-host, но упал в
host-regression: test_e2e3_original_new_game импортирует генератор как Python
package, а новые sibling imports E2E-5.7/5.8 поддерживали только script mode.
Исправлено разделением relative imports/package и absolute imports/script.
Ровно отказавшая make-команда локально прошла; повторный combined preflight
PASS, все 11 сохранённых SHA effective gameplay/file/thread sources совпали
с паспортом NRO. Игровой C++ и готовый артефакт не менялись; повторная NRO
не собиралась. Исправление и дополнительные результаты отправляются новым
коммитом для повторного полного CI.

Следующий аппаратный запуск: A) меню → аркада, stick directions/диагонали/180°,
right stick/сенсор/ZR/ZL, Options/имя; B) одна новая игра с зафиксированными
seed/настройками, стадии/фон/курсор/Next → RuinsTalk/ангар/магазины;
C) взлёт через 51% → видимая StarMap + управление кораблём/игровое действие.
При первой критической остановке сохранить первый журнал и дамп; повторную
многоминутную генерацию без анализа не запускать. Считать 51% и freeze
закрытыми только после этого прогона.

Отчёт приставки должен включать полный SHA NRO/Build ID, stage timings,
arcade-phase/projection hits, thread/core affinity, loader begin/end и очередь,
последние UI/music записи, память, наличие crash и точный достигнутый экран.
Текущий статус: **PARTIAL / HARDWARE PENDING**.

MatrixGame: [ABI, зависимости и MG-1/2/3](../../docs/matrixgame-switch-plan.md).
