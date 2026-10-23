# Офлайн-стабилизация по аппаратным данным 23.10.2026 — продолжается

Это продолжение анализа `hardware-20261023-analysis.md`, не объявление устранения
всех дефектов. Главные источники: неизменённые `logs/2026.10.23`, frozen E2E-5.16
и pinned исходники. Основная NRO/ELF и оба старых ZIP не пересобираются.

Базовый implementation commit диагностики: `be6f63e11ab2c434336502a38496ec7d19e5b0bb`,
локальный master, без push/fetch. Команды, source/object hashes, исходы и честные
границы сохранены в приватном `build/e2e523/evidence.json`. Повторно совпали все
107 входных файлов аппаратного прогона и 13 доступных защищённых baseline файлов;
`G:\pack.zip` остаётся недоступен, его текущий hash не объявляется проверенным.
Оба frozen ZIP совпали с прежними SHA-256. Пользовательские данные не включены в Git.

## 1. Повторяющийся текст: источник ещё не найден

Из аппаратного stage log дополнительно выделен точный ресурс испорченного
фона: **`GI,Bm.FormRuins.2RCbg`**. Он наблюдается у RuinsTalk, GoodsShop, Info и
Hangar; во всех этих записях `hardware=0`, bounds1280x720. Это исключает гипотезу,
что настройка в этом переходе случайно включила D3D-ветку. Свежие CPU-холсты
перед композицией чистые; фрагмент вводится оригинальным рисованием интерфейса.

Выполнен один новый source-backed host baseline с реальным прокрученным текстом:
generation → ожидание20с → разговор → ангар → карта/меню. Ожидание добавлено в
существующий raw-pad driver через `--intro-hold-seconds`; код генерации, RNG,
OnClose и рисование не изменены. Текст действительно присутствует на обоих
снимках генерации, что проверено просмотром PPM. После перехода на стартовую
станцию текст не остаётся. Но там выбран другой станционный фон: это не
доказательство исправности именно аппаратного `2RCbg`-маршрута.

Артефакты: `build/e2e515/e2e523-intro-baseline/result.json` и
`build/e2e523/intro-baseline-contact.jpg`. В этом драйвере PASS означает маршрут;
он завершает процесс принудительно, `normal_shutdown_verified=false`.
Существующий baseline binary SHA256
`4e826f1076dadeba796ec89dee7750190b4caa110862ae5f04a7a77eed18fb81`.
Это старый Switch-effective host executable, не вновь собранный кандидат с
изменённой диагностикой. Графический fix и очередная очистка framebuffer
**не внесены**. Следующий адресный шаг — источник `2RCbg` и соответствующий
CPU/cache buffer в точном переходе, а не общий новый цикл всех рендер-тестов.

### Уточнение пользователя и адресный тест `2RCbg`

Пользователь отдельно подтвердил по приложенным снимкам: экран генерации мира
повторяется **на экране планеты**. Это часть исходного дефекта переходов между
экранами; его нельзя сводить к аркаде или к зависшему последнему кадру.

Предварительная проверка была выполнена на локальном
`DATA/FormRuins/2RCbg.gi`: исходный Format0 RGB565 1920x1536,
центрированный crop1920x1080 с top228, выход1280x720.
**Уточнение:** это одноимённый ресурс из `forms.pkg`, а не фактический источник
ключа `Bm.FormRuins.2RCbg`. Приведённый ниже Format0 PASS не доказывает исправность
показанного на Switch фона. Правильный источник установлен далее.
Оригинальные `TgiGR::DecodeToGraphBuf`, `TGraphBufGR::RescaleRGBA_HW`
(существующий CPU override), `CreateFromGraphBuf`,
`TCGiEC::ApplyWideScreenLayoutFixups` и `LoadFromConfigBuffer` прошли проверку.
Декодирование сверено с Python-декодером, rescale с прямым вызовом того же OKGF,
а RGB565 — с независимой формулой каждого пикселя. CacheGI сохраняет собственную
копию после очистки и освобождения входного `TBufEC`.

FNV64: decoded `bfd1c77c6dbe61fd`, scaled `fda4ed7c51d55c3e`,
packed `9ca6adbf22277927`. Полезная нагрузка выхода1843200 байт,
весь GI1843296. Артефакты: приватные
`build/e2e523/check-ruins-resource.py`, `ruins-resource/result.json`,
`ruins-resource/build.json`, `ruins-resource.log`.

Первое сравнение полного GI выявило различия только на смещениях88..95:
это `TgiPlaneGR::cpp_padding[8]`, неинициализированные при создании GI и
не читаемые декодером. В адресном fixture сравниваются все значимые поля
заголовка и **все** пиксели; отдельно проверена копия после освобождения input.
Игровой код ради заполнения padding не изменялся. Прежний общий fixture не
перепрогонялся и не объявляется заново проверенным.

Граница: **HOST resource/cache slice PASS**, без ASan/ARM64/аппаратного прогона
именно этого нового кандидата. Fixture перенаправляет OKGF wrappers напрямую
в локальную библиотеку; production resolver, полный UI, экранная композиция и
порядок переходов этим тестом не проверяются. Совпадение host-хэшей не является
cross-architecture доказательством.

Расхождение5898336/5968179 **объяснено разрешением ключа**, а не объявлено порчей:
оригинальный `CFG/CacheData.dat` отображает `Bm/FormRuins/2RCbg` в
`data\FormRuinsBG\2RCbg.gi`. Это **Format2**, три плоскости, из `background.pkg`,
ровно5968179 байт. Такой же размер уже присутствует в полном host-запуске
`e2e523-newsave-candidate`. SHA256 локально извлечённого фактического GI:
`6de1e1ef78679d017f78fe52b4f02c0f7e51cad71cf41d1d483a4cf9f1030ef3`.
Хэш самого аппаратного ресурса не прислан: побайтовая идентичность не доказана.

Адресный тест повторён **на фактическом Format2** с независимым Python RLE
декодером и оригинальными decode/rescale/repack/CacheGI методами. HOST/NATIVE
PASS: decoded FNV64 `63fb890d7dc66568`, scaled `e36b6f814bc16840`, packed
`c7abbe2c80d5e0f4`. Своя копия CacheGI сохраняется после освобождения входа.
Артефакты: `build/e2e523/ruins-format2/result.json`, `ruins-format2.log`.

### Подтверждён отказный путь публикации незаписанного буфера

В pinned `OKGF_Rescale` необходимы четыре calloc (таблицы/веса) и один malloc
(промежуточные строки). При отказе он освобождает временные блоки и **молча
возвращается через void ABI, не записав destination**. Оригинальный
`RescaleWithAspect` затем освобождает источник и принимает незаписанный
destination с новыми размерами. В resource cache могут попасть остатки прежнего
содержимого heap, без какой-либо ошибки загрузки ресурса.

Это воспроизведено на фактическом Format2: избирательно отказало каждое из пяти
выделений **только внутри оригинального rescale.c**. Во всех случаях весь выход
остался тестовым0xcd, но объект уже имел1280x720 вместо1920x1536.
`build/e2e523/rescale-failure/result.json` содержит исходные сбои; это отказная
проверка, а не PASS старой реализации.

В новом кандидате:

- `Ex_OKGF_Rescale` изолирует errno вызова. Required allocation failure ENOMEM
  из POSIX/newlib становится ошибкой; старый ENOENT/ENOMEM вызывающего кода не
  используется как результат и восстанавливается даже при исключении.
- `RescaleWithAspect` при исключении освобождает ещё не принятый destination
  и освобождает полученные texture locks; **исходные пиксели, размеры и
  владение не меняются**. Незаписанный результат не доходит до repack/cache.
- В generic background-ветке `TCGiEC::ApplyWideScreenLayoutFixups` временный
  `SourceGraph` теперь также имеет scope owner: исключение rescale не оставляет
  исходный RGBA временный буфер без владельца. Нормальный порядок очистки
  сохранён; после явного освобождения указатель сбрасывается.
- Алгоритмы фильтрации, формат GI, ресурсы, RNG и обычная аркада не меняются.
  Дополнительной очистки framebuffer нет. В первом guard проверялся успешный
  последующий вызов после снятия отказа; ограниченный recovery добавлен ниже.

HOST/NATIVE ASan/UBSan/leak-check PASS для действительного generated
`Ex_OKGF_Rescale` (извлечённого из candidate `GR_Main.cpp`) и original generated
`GR_GraphBuf.cpp`: обычный вызов и пять отказов с сохранением источника и
последующим повтором. Результат повтора побайтово тот же, что у нормального
вызова. Полные результаты: `build/e2e523/rescale-candidate/result.json`.
Отдельный tracked `test_e2e523_checked_rescale.cpp` проверяет errno и исключения.
Оба изменённых полных effective units скомпилированы devkitA64:
`rescale-candidate/sdk-result.json`; **COMPILE PASS, без link/NRO/hardware**.

Граница причинности: аппаратные записи показывают большую malloc arena и
почти исчерпанный uncommitted tail, но это **не доказывает отказ именно этого
malloc/calloc**. Большие free_blocks также не доказывают наличие достаточно
большого непрерывного блока; top_free не является максимальным свободным блоком.
Новый guard исправляет доказанную ошибку публикации при ENOMEM, однако источник
именно аппаратного текста пока **OPEN**. Первый PASS использовал NATIVE math;
frozen Switch использует prepared fast24 COMPATIBLE/PORTABLE_MATH/no-TLS.
Эта граница дополнительно проверена отдельной host-библиотекой из **самого
prepared fast24 source** с `COMPATIBLE/PORTABLE_MATH/no-TLS`, точностьGAME24.
На реальном Format2 оригинальные decode/rescale/repack/CacheGI методы прошли
сверку с независимым RLE декодером и прямым вызовом того же backend:
scaled FNV64 `8e98fdb0ba945a29`, packed `3c05d2ffbfc58bac`.
Это ожидаемо другие хэши, чем NATIVE; равенство разных backend не утверждается.
Артефакт: `build/e2e523/ruins-format2-compatible/result.json`.

На этом же prepared backend прошла адресная ASan/UBSan/leak regression
с generated original `Ex_OKGF_Rescale` и `RescaleWithAspect`: нормальный вызов,
каждый из пяти внутренних отказов, сохранность исходника и эквивалентный повтор.
Регрессия использует синтетические пиксели и не требует коммерческих ресурсов;
ресурсный и отказный тесты имеют разные границы. Артефакт:
`build/e2e523/rescale-portable-regression/result.json`.
Воспроизводимый tracked runner (Linux/WSL, из корня checkout):

```sh
python3 tools/e2e523_rescale_failure.py \
  --library-build build/e2e523/okgf-compatible-host \
  --rescale-source port/switch/build/e2e-game-perf515/okgf-fast24-source/src/rescale.c \
  --output build/e2e523/rescale-portable-check
```

Для allocator instrumentation объект rescale перелинкован с matching скрытыми
SoftFloat helpers из этой же сборки; алгоритм rescale не переписывался.
Успешный запуск записан после добавления этих обязательных link dependencies,
остальные готовые объекты при ремонте link повторно не компилировались.
Это **HOST PASS на соответствующем backend**, не ARM64 runtime и не Switch PASS.
Общее происхождение источника и compile PASS не заменяют аппаратной причинности.

После проверки вызывающего слоя расширена **та же адресная regression**:
`LoadFromConfigBuffer` на фактическом Format2 из `background.pkg` выполняется
обычно, затем с каждым из пяти внутренних отказов и повтором. При отказе
входной GI не меняется, Image не публикуется; после повтора пиксели совпадают
с нормальным cache baseline и остаются после освобождения входного `TBufEC`.
ASan/UBSan/leak-check PASS включает временный `SourceGraph` вызывающего слоя.
Артефакт: `build/e2e523/rescale-cache-regression/result.json`.
Tracked runner поддерживает `--gi` для частного извлечённого ресурса;
без него создаётся синтетический Format0. Сам коммерческий GI в Git не включён.
Проверяются прямые оригинальные LoadFromConfigBuffer/ApplyWideScreenLayoutFixups,
**не** конкурентный глобальный AcquireDataFromConfig и не полноценный UI после
неудачной загрузки. Это отдельная оставшаяся граница error recovery.
Дополнительно guard EC_CacheGI материализован через production UI/trace/perf
шаги и скомпилирован devkitA64: `rescale-cache-regression/sdk-result.json`,
**COMPILE PASS**, без link/NRO/hardware. Таким образом, проверены три изменённых
SDK translation units; прежние два объекта при проверке третьего не повторялись.

### Ограниченное восстановление при отказе rescale

Прямые malloc/calloc внутри OKGF обходят уже существующий резерв native heap.
Поэтому доступность обычных игровых выделений не доказывает, что rescale
смог получить свой промежуточный блок. Для crop1920x1080 →1280x720 он требует
5529600 байт; отдельно выделяется destination3686400 байт.

В effective `Ex_OKGF_Rescale` теперь используется существующий
`memory_recovery::Reclaim("ui-background-rescale", ...)`: только после ENOMEM
освобождаются не удерживаемые элементы LRU. Повтор допускается **ровно один раз**
и только если reclaim действительно уменьшил resident cache. При повторном
отказе действует описанная выше транзакционная защита источника. Прямого
доступа к резерву в обход его mutex и глобальной замены allocator нет.
Исходный errno сохраняется также при recovery callback и его исключении.

Адресная ASan/UBSan/leak regression использует реальный Format2, generated
Ex-wrapper и оригинальные GraphBuf/CacheGI методы на prepared compatible
backend. В ней выполнены настоящий `Reclaim`/`TCacheEC::TrimToBudget`,
освобождение не удерживаемого GI и сохранение loader entry с RetainCount=1.
При одном отказе kernel вызывается дважды, конечные пиксели совпадают с
обычным cache baseline. При двух отказах вызовов также ровно два, Image
не публикуется, входной GI неизменён. Отдельно проверены отказ recovery и
исключение callback с восстановлением errno. HOST PASS:
`build/e2e523/rescale-recovery-regression/result.json`, `errno-test.log`.
Полный SDK `GR_Main` на frozen production основе с тем же исполненным
Ex-wrapper скомпилирован: `sdk-result.json` в этом каталоге. Это **COMPILE
PASS**, без нового link/NRO и без аппаратного подтверждения.

### Сопоставление двух аппаратных процессов

Просмотрены обе аппаратные contact sheets: в первом процессе intro-текст
остаётся на RuinsTalk/Goods/Info/Hangar, но карта чистая; во втором после
загрузки того же AutoSave станция и ангар показывают **чистую лабораторию**.
Следовательно, постоянное содержимое ресурса или сериализованная картинка
сами по себе не объясняют оба результата. Фактический GI также визуально
содержит лабораторию, а не экран генерации мира.

Для одного `Bm.FormRuins.2RCbg` интервал от header до успешного repack/cache:

| Процесс | Изображение | Интервал | Ближайшие heap данные до загрузки |
|---|---|---:|---|
| первый | с текстом intro |92.272мс|arena3311820800, used629533248, free_blocks2682287552, uncommitted2715648|
| второй | чистая лаборатория |446.137мс|arena720814080, used580246144, free_blocks140567936, uncommitted2593722368|

Это согласуется с пропущенной работой при недостатке непрерывной памяти,
но **не доказывает конкретный отказ malloc**: free_blocks — сумма, не размер
максимального блока; время включает декодирование и repack, не один rescale.
В старом логе нет результата каждого allocator call или пиксельного hash
самого GI draw. Причина именно аппаратного повторного текста остаётся OPEN.

Frozen NRO/ELF, оба ZIP, пользовательские сохранения и игровые ресурсы
не изменялись. Новая NRO не создавалась. После проверки повторно совпали все107
входных аппаратных файлов и13 доступных protected baseline файлов, включая
старый GPU ZIP; также отдельно совпал frozen основной ZIP.
Сверка: `build/e2e523/rescale-protected-after.json`. `G:\pack.zip` недоступен.

## 2. manual-publish: точный аппаратный syscall ещё неизвестен

Старое сообщение объединяло source fopen, destination open/fdopen, read/write,
flush и close. В старых ветках open/fdopen error_code уже фиксировался до
cleanup; для fread errno/ferror не сохранялись до последующего fwrite.
Старый общий текст не различает эти случаи. Теперь сохраняются:

- точная фаза (`source-open`, `destination-create`, `destination-fdopen`,
  `read`, `write`, `flush`, `destination-close`, `source-close`, final stat);
- исходный error_code до cleanup и число записанных байтов;
- UTF-8 bytes обоих путей в hex, без потерь ANSI и внедрения переводов строки.

На успешном пути нет нового журналирования или форматирования путей. Byte I/O,
O_EXCL, валидация, flush/readback, rollback и публикация commit сохраняются.
Это **исправление точности диагностики**, не доказанное устранение аппаратного
ENOENT. Исходное сообщение само по себе не позволяет выбрать одну фазу.

Новый тест `test_e2e523_manual_publish.cpp` использует **настоящие присланные
RSG64 поколения** и production ReplaceGameSavePath. Четыре случая: source-open
ENOENT и destination-create ENOENT × новый/занятый `Солнце.sav`. Внедряется
только отрицательный результат libc, не успешное копирование. Проверены
различение фаз при одинаковом errno2, bytes имени, неизменность temp/старого
слота, настоящее восстановление, успешный Unicode retry и следующая запись.
После успешного commit тест создаёт новое временное поколение, как TSaver:
старый save.tmp должен быть освобождён, переиспользовать удалённый файл нельзя.
Host ASan/UBSan/leak detection — PASS. Входные сейвы не менялись.

Дополнительно прочитаны заголовки присланных файлов без изменения данных:
AutoSave и backup содержат turn301 и описание станции, RecoveredSave-1 и
save.tmp — turn303 и описание `Система`. Описание внутри RSG64 **не является
именем целевого файла**, поэтому по нему нельзя восстановить введённое имя
или target path двух неудачных публикаций. Каталог `.e2e4-manual-transactions`
в присланных данных пуст. Артефакт: `build/e2e523/save-header-evidence.json`.
Обе ошибки произошли во втором, графически чистом процессе, где reserve
recoveries оставались0 и большой uncommitted tail сохранялся. Объединять их
с rescale ENOMEM на основании этих данных нельзя.

## 3. Настоящий load → ходы → ручная запись на новых данных

Копия нового AutoSave загружена в существующем полном host executable через
исходный UI. Наблюдаемый checkpoint: turn301, money980, ship2, star3, planet0,
16 scripts, generation_seed908803244. Три настоящих хода дали turn304;
оригинальный UI создал физический Unicode-файл `Солнце.sav`.

Первый rig ошибочно ждал commit marker в legacy `########.log`, который у
Switch-effective performance профиля содержит только Start. Тот же marker
**уже был** в perf515-stage.log, а правильный UTF-8 файл существовал. Это был
таймаут harness, не воспроизведение аппаратного ENOENT. Исправлен выбор журнала:
сохраняются тот же commit marker, проверка существования/размера именно
ожидаемого файла и сравнение сохранённых checkpoints. Не заменено условием
«любой файл существует».

Один повтор кандидата с исправленным чтением журнала — PASS:
`build/e2e515/e2e523-newsave-candidate/result.json`. Это исправление rig, а не
игровой файловой системы. Переход выполнен на станции/ангаре: **не следует
считать его воспроизведением полного зависания общей карты**.

Fresh-process reload завершён **PASS**, а не выведен из CRC:
`build/e2e515/e2e523-newsave-reload/result.json`. Новый процесс действительно
загрузил `Солнце.sav`, turn304; checkpoint_comparison_verified=true.
С сохранением совпали RNG, scripts/globals, inventory и ship hashes, money980,
ship2/star3/planet0. SHA файла
`924ec8ee27519ab9a5ac8b3871330e55d5b3a4377e87a2a0924b0826af0b8f7d`.
Новый AutoSave source SHA
`87bf6b2a50b77826eb9f1941d815280978bdab2dcc7f1f0550416a4e2813ca3d`.
Драйвер затем остановил процесс (-9), поэтому это всё ещё не проверка штатного
выхода. Файловый путь работает в этой host-среде на новых реальных данных;
аппаратный ENOENT остаётся отдельной незакрытой причиной.

## 4. Раздельная диагностика карты и выхода

В существующий perf writer добавлены четыре статических atomic slots: main,
turn, saver, writer. Публикация не берёт игровые mutex, не выделяет память и
не пишет файлы. Только существующий writer раз в heartbeat выводит не более
четырёх дополнительных строк. Snapshot имеет bounded retry и consistent flag;
прогресс записей в очереди не используется как число кадров.

На Switch thread_token получен через svcGetThreadId, на Linux — gettid; fallback
явно помечается native_id=0. Для каждого role выводятся phase/stage, since/age и
монотонный progress. Слоты сбрасываются при новом Initialize. Они отражают
последнюю опубликованную работу, а не гарантируют, что OS-поток всё ещё жив.

Точки:

- main: исходные loop/present Stage IDs; ожидание turn-idle; `osk-active` на
  всё время настоящего системного applet. Большой SaveManager interval с этим
  маркером нельзя объявлять CPU-зависанием;
- turn: вход оригинального TThreadCalc_Execute с настоящим Self->Job;
- saver: ожидание SaveLoadLock → serialization → именованная copy transaction;
- exit: после возврата ProgramMain, перед return main, перед drain/join writer;
  writer отдельно подтверждает выход из drain loop. Этот последний marker
  **не** утверждает завершение fclose, всех static destructors или возврат hbmenu.

Новый тест `test_e2e523_thread_progress.cpp` блокирует настоящий worker на
condition_variable, различает его native ID/неизменный progress и фазу main OSK,
проверяет restore scopes, heartbeat, bounded join и повторный Initialize без
stale worker identity. ASan/UBSan/leaks — PASS. Существующая регрессия
async producer I/O guard/pressure/shutdown тоже PASS. Это не воспроизведение
аппаратного system freeze. Если перестанет работать весь kernel/SD, обычный
writer не сможет гарантировать последнюю запись — аппаратная граница остаётся.

## 5. ARM64 и неизменность игры

Адресная devkitA64 компиляция с performance diagnostics=1 — PASS для семи
затронутых частей: perf.cpp, save transaction, оригинальные подготовленные
ThreadCalc/aSaveLoad/program/GI_MessageLoop и реальный OSK backend. Все новые
overlays лежат отдельно в `build/e2e523/sdk-source`, frozen effective sources
не перезаписывались. Это **COMPILE PASS, не полный link/NRO/runtime PASS**.
Standalone OSK probe выявил отсутствовавший прямой include perf header;
исправлено и повторён только этот unit. Уточнённый Self->Job также проверен
повторной компиляцией только ThreadCalc.

SwkbdType_All и прежнее исправление deko image alignment сохраняются. Нового
аппаратного подтверждения нет; старый smoke имеет HARDWARE FAIL до draw,
его исправленный вариант — HARDWARE_PENDING. Аркадные алгоритмы/тик/ввод не
оптимизировались и не менялись. MatrixGame E2E-5.19 остаётся после первоочередных
P0 основной игры.

Работа продолжается: (1) конкретный владелец/источник `2RCbg`-композиции;
(2) аппаратный ENOENT до сих пор не локализован до syscall;
(3) точные причины freeze карты и полного выхода не установлены. Новые markers
позволят различить их при будущем доступе, но новый Switch-тест сейчас не требуется.
