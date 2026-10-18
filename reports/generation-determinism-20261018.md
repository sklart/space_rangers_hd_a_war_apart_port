# Генерация: host repeatability и контроль пакетного reader

Область: один seed/набор настроек, реальные оригинальные ресурсы, все 300 прогревочных
ходов. Это не аппаратный тест Switch и не подтверждение ускорения генерации.

## Метод и границы наблюдения

В source overlay добавлен только host guard E2E5_HOST_SDL_WINDOW.
E2E511_HOST_GENERATION_SEED=314159 задаёт начальный RTL input вместо Randomize и
повторяет штатное присваивание GenerationSeed/RandomState из custom seed.
Алгоритм RNG, его draw, число ходов и порядок расчётов не заменяются.
Без opt-in seed исходное поведение сохраняется. На Switch весь helper исключён.

После начального galaxy calculation, каждого 25-го завершённого хода и финальных
настроек игрока пишутся 14 записей: seed, CurrentTurn, RNG галактики/RTL, количества
и ключевые поля/RNG всех звёзд, планет, кораблей в секторах и транзите. В этих
промежуточных записях FNV-1a хеширует именно перечисленные ключевые поля, а не весь
world graph. Имена и порядок объектов включены. elapsed_us отделён от состояния.

Для полного мира отдельно вызывается настоящий TGalaxy::SaveToBuffer уже после stage 8.
Он НЕ read-only: native OnSave handlers и SaveCount выполняются, после чего диагностический
процесс прекращается, дальнейшей симуляции нет. В приватный каталог прогона пишется
полный оригинальный serialized payload generation-world.bin. Его SHA-256 считается по
всем байтам без удаления/нормализации полей. Это полный persisted-world payload данного
сериализатора, не raw RAM/caches/worker graph. Запись Switch saves по-прежнему выключена.

Native OnSave в каждом из шести прогонов: galaxy RNG 1518858239→1610087023,
RTL 1471877872→1471877872, SaveCount 0→1. До/после записаны отдельно; вызов сохранения
не объявляется пассивным наблюдением. Промежуточные checkpoints не вызывают SaveToBuffer.

## Обнаруженные различия входов и RNG boundary

Две первые пробы с пустым полем имени выбрали разные имена в меню до создания мира.
RNG/звёзды/планеты совпали, но ship_key_hash и полный payload не совпали. Первое различие
payload оказалось в имени игрока. Эти результаты НЕ выданы как PASS и сохранены в
build/determinism-run1.json / run2.json. Имя не вырезалось из хеша.
Для повторяемой серии штатный newgame.txt скопирован через --new-game-file с явным
Name=Determinism. Остальные настройки взяты из того же native config; все input hashes
и конфиги совпали. seed=314159, levels=0, язык russian.

Наблюдение RTL сразу после конструктора в трёх candidate runs:
834181143 / 1485582739 / 834181143. Это состояние до native GenerateGalaxyLayout: upstream/cpp/src/aGalaxy.cpp явно выполняет
System::RandSeed = Galaxy->GenerationSeed перед созданием созвездий.
Поэтому эта ранняя величина сохранена отдельным observation, а не названа одинаковым
исходным состоянием готового мира. Во всех 14 completed-world checkpoints RNG сравнивается
строго, включая RTL. Ни одно значение RNG в этих checkpoints не исключено из сравнения.
Не заявляется полная детерминированность всех UI/constructor interleavings процесса.

## Настоящий old/new контроль

Candidate ELF: 3e2c1539c5cd0a06faddf280b0c193793f35ae4661dc09cdc7dbedb88dd3449e.
Control ELF: f35226709b26e8ce756d3eee3c4ce128bd1b82c65ee64d3064fcb2177749d31c.
Control использует точные package.hpp/package.cpp из локального commit
0f91c1ab48328f187e10f58682d318b2487176bd. Проверена зависимость от package.hpp во всех
331 object depfiles: только package и ec_file_adapter. Оба пересобраны вместе с одним
старым header, 329 остальных объектов переиспользованы. Это предотвращает смешивание
layout std::vector<Package::OpenEntry>. Native generator, seed instrumentation, flags
и OKGF совпадают. Control не откатывает весь порт к исходному commit и не меняет checkout.
Builder: tools/e2e5_legacy_package_control.py; provenance в
port/switch/build/e2e5-host-game/legacy-package-control/report.json.

| Host generation, от fixture input до final checkpoint | run 1 | run 2 | run 3 | median |
|---|---:|---:|---:|---:|
| Новый bounded streaming | 11.204171 с | 11.598790 с | 12.035804 с | 11.598790 с |
| Исходный whole-payload reader | 9.648723 с | 9.461698 с | 9.465149 с | 9.465149 с |

Все шесть full payload: 2094630 байт, SHA-256
`a6183264eb0d9c55f533a0fb4fae0fb94f1d666f0376f0ee18e0df47961faf61`.
Все 14 контрольных записей мира/RNG, native save transitions и input hashes совпали
внутри каждой тройки и между old/new. Постоянное состояние и выбранные RNG checkpoints
для этого сценария эквивалентны. Эти данные не заменяют A4 ownership/history audit.

В данной последовательной серии bounded вариант медленнее примерно на 22.54%.
Candidate выполнялся раньше control; частоты, температура и внешняя нагрузка ОС не
измерялись. Причинность замедления полностью не установлена. Даже без утверждения
причинности эти времена НЕ дают основания объявлять ускорение или закрыть P1.
Memory benefit (без лишней полной копии) подтверждён отдельно, но заметная host
разница требует проверки I/O/повторного декодирования до принятия performance результата.
Никаких новых NRO ради этих сравнений не создано.

## Проверяемость

- tools/e2e5_compare_generation.py требует три разных completed run directories на группу,
  normal snapshot exit, 14 упорядоченных checkpoints, полный размер payload, unique marker,
  одинаковые input/binary внутри группы и строгую эквивалентность old/new.
- 8 negative/positive tests: elapsed time отдельно; изменение world bytes, checkpoint RTL,
  off-screen entity key, input config, duplicate run, missing marker и truncation отклоняются.
- Проверки comparator включены в обязательный e2e5-fast CI job. Native resource/benchmark
  runs в публичном CI не имитируются: закрытые данные отсутствуют.
- build/determinism-package-comparison.json содержит полные hashes/параметры/времена.
  Raw payload/config остаются в ignored build/runs, не в Git/CI artifacts.

Обычный и combined ARM64 compile/link/audit повторены после добавления host helper:
undefined/stubs=0, UNKNOWN=0. Manifest изменился только на один диагностический source
и номера callsite. В executable sections двух изменённых игровых единиц aGalaxy и
fGameSettings до/после совпали все 1348 секций: ordinary 594+71, combined 612+71.
Логи build/{ordinary,combined}-determinism-link.log,
сравнение build/arm64-host-determinism-code-comparison.json.
Это проверка machine bytes этих объектов и link, а не аппаратный функциональный PASS.

## Воспроизведение

1. Собрать optimized host существующим tools/e2e5_host_game_build.py.
2. Подготовить один native newgame.txt с явным именем; каждый запуск получает его копию
   через --new-game-file в свежем isolated user/config.
3. Трижды последовательно запустить обычный tools/e2e5_host_game_probe.py с
   E2E511_HOST_GENERATION_SEED=314159 и E2E511_HOST_WORLD_SNAPSHOT=1,
   --auto-start-game --continue-after-frame, с теми же --cfg-file/--lang-file/ресурсами.
4. Выполнить tools/e2e5_legacy_package_control.py в WSL после готового host build;
   повторить три пробы с его binary и той же текущей library-dir/настройками.
5. Передать report.json каждой пробы в --candidate (3 paths) и --baseline (3 paths)
   tools/e2e5_compare_generation.py. Exit 0 означает только заявленную host equivalence;
   наличие ускорения оценивается отдельно по временам и аппаратным замерам.


После инструментации повторён обычный маршрут без seed/snapshot env:
build/host-after-determinism-normal.json, тот же binary SHA 3e2c1539c5cd0a06faddf280b0c193793f35ae4661dc09cdc7dbedb88dd3449e.
114.28 с, crashed=false, timed_out=false, generation_complete=true, экраны 20/4,
gameplay_60_frames=true, stopped_by_probe=true. Выполнены optional music OOM,
два heartbeat и raw touch. В каталоге нет generation-world.bin/checkpoint outputs,
в логе нет fixture seed/snapshot marker: opt-out не выполнял диагностическое сохранение.


Повтор после bounded seek cache: три полных payload и все 14 checkpoints снова
совпали с original-package control. Времена 9.308419 / 12.247024 / 11.424134 с;
медиана 11.424134 с против 9.465149 с (+20.70%). Performance gate остаётся OPEN.
Меньше stdio seek не означает доказанного ускорения полной генерации.
Подробности: package-seek-20261018.md; build/determinism-seek-vs-legacy.json.


## Текущий reserve-resize host: чередующийся A/B контроль

После изменения общего heap helper полностью пересобраны 316 зависимых единиц,
затем ordinary host route прошёл до ангара, real EC_Cache ASan/UBSan — два fault cases.
Для A6 создан отдельный legacy-package-reserve-control, не перезаписав ранний f352 control.
329 переиспользованных объектов и flags/OKGF совпадают у текущей пары; package.cpp
и ec_file_adapter.cpp пересобраны вместе с точным old header. Report в
port/switch/build/e2e5-host-game/legacy-package-reserve-control/report.json.

Candidate SHA: `8890e70bf7db7983e98e8dfc4583325e08c165ffcef9a1b48f9b22026d07e5e1`.
Control SHA: `a8ebf3225ae93f5c8ef1fc9de0aef61f76a58a8f1222cd1db143ca2f4394278e`.
Без других тяжёлых сборок/test jobs запуски чередовались C1/B1/B2/C2/C3/B3,
с теми же seed=314159, именем, inputs и оригинальными ресурсами.

| Группа | Run 1, с | Run 2, с | Run 3, с | Median, с |
|---|---:|---:|---:|---:|
| Bounded + seek cache + reserve resize | 11.324724 | 10.613575 | 12.851684 | 11.324724 |
| Whole-payload control, same new heap/game objects | 10.476147 | 10.395240 | 10.723823 | 10.476147 |

Все шесть полных payload 2094630 байт, SHA-256
`a6183264eb0d9c55f533a0fb4fae0fb94f1d666f0376f0ee18e0df47961faf61`.
Все 14 world/RNG checkpoints, inputs, native save transitions строго совпали.
Comparator PASS: build/determinism-reserve-paired-comparison.json.
Лог последовательности: build/reserve-generation-paired.log.

В этой серии median ratio 1.081000868: candidate медленнее примерно на 8.10%.
Ускорение не принято, performance OPEN. Разница меньше старых последовательных
22.54/20.70%, но ранние результаты не удалены и не названы ошибочными замерами.
Частоты/температура/внешняя нагрузка ОС не измерялись; чередование уменьшает order bias,
но не доказывает причину остаточной разницы. Промежуточные wall-time интервалы
сохранены в build/determinism-reserve-checkpoint-intervals.json; это не CPU profile.
Актуальная host repeatability подтверждена, Switch performance/батарея остаются OPEN.


Разделение уже записанных checkpoint wall times (без дополнительных запусков):

| Интервал | Candidate median, с | Control median, с |
|---|---:|---:|
| Fixture input → checkpoint 0 | 2.288541 | 1.723600 |
| Checkpoint 0 → checkpoint 300: ровно 300 warmup turns | 8.546326 | 8.534197 |
| Checkpoint 300 → final checkpoint | 0.308307 | 0.199947 |

Checkpoint 0 записан после initial CalculateGalaxyTurnAndWait; до него входят
конструктор, подготовка мира/экрана и этот первый расчёт. Отдельные начальные времена
candidate 2.288541 / 1.758942 / 4.093104 с, control 1.723600 / 1.752677 / 1.719582 с.
Это локализует крупную вариацию до 300-ходового цикла; причинность package I/O,
UI/worker scheduling или allocator effects ещё не доказана. У начального RTL есть
ранее описанная constructor/UI вариация до native layout seed reset.
Медианы отдельных интервалов не складываются в медиану общего времени.
Для самих 300 ходов ratio около 1.00142; ни ускорения, ни формальной статистической
эквивалентности времени по n=3 не заявлено. Следующий профиль нужен для подготовки,
а не упрощения расчётов 300 ходов. Данные build/determinism-reserve-phase-summary.json.
