# Стабилизация 18.10.2026 — промежуточный результат

Карта всех gates ТЗ: [stabilization-gates-20261018.md](stabilization-gates-20261018.md).

Статус: **PARTIAL / HARDWARE PENDING**. Это не подтверждение устранения аппаратного зависания и не готовый релиз.

Исходная ревизия: `0f91c1ab48328f187e10f58682d318b2487176bd`.
Изолированная ветка: `codex/stabilization-20261018`.
Основной dirty checkout, оригинальные ресурсы и пользовательские сохранения не изменялись.
Pinned cpp/okgf/MatrixGame сохранены.

## Аппаратный отказ P0.1

Архив `pack.zip`: SHA-256 `a37c1c8b43effb3a9dfccc82596f4bd41aa17682342cbc3d6cda478dfe4bcd18`.
Stage 8 зарегистрирован за 377757 мс, затем `continue-enabled`; перехода с экрана 3 нет.
Нового Atmosphere crash report в этом архиве нет.

Пользователь уточнил: после «Готово» экран визуально замер, курсор перестал двигаться;
навести курсор и выполнить A/тап по «Далее» не удалось из-за замершего UI. Это нельзя описывать как простой промах мимо кнопки.
В журнале при этом продолжаются input poll/dispatch и цикл кадров.
Их наличие не доказывает работу игрового UI и обновление видимого курсора.

Host на оригинальных ресурсах, включая аппаратные CFG/Lang, проходит настоящие
300 ходов → активная кнопка → mouse down/up → callback → RuinsTalk (20) → ангар (4).
Измеренный host-хитбокс: (1222,638)–(1263,708), active=1, disabled=0,
generation_stage=8, worker_running=0. Аппаратную геометрию этим не подтверждаем.

В старом логе подробная диагностика callback ограничена первыми событиями процесса:
лимит исчерпан до Introduction. Новая диагностика сбрасывает лимиты при смене экрана
и стадии; записывает вход/возврат GI обработчика, touch, raw координаты,
MemorySnapshotActive/ExitCode, active root/cursor, IgnoreWarpMouseMove и позицию курсора
после обработки. Кнопка Ok отдельно пишет hit/hover/occlusion/disabled/callback.
Автоматического продолжения и обхода обязательных расчётов не добавлено.

**Первопричина аппаратного отказа: UNCONFIRMED.**

## Память и пакетные ресурсы P0.2

**CONFIRMED (код):** первый ReadEntry раньше распаковывал весь файл в std::vector,
в обход native heap reserve. Теперь чтение потоковое: 65536 байт decoded + 72112 байт
packed на открытый compressed reader, плюс FILE/метаданные/внутренние выделения zlib.
ReadPayload оставлен независимым контрольным путём. Seek назад, переход через границу
блока, два readers, EOF и ошибка позднего блока проверены. FILE принадлежит RAII owner.

ASan/UBSan: synthetic stream/random seeks, повреждённые пакеты/циклы; настоящая
TFileStreamEC integration с OOM, ошибкой позднего блока, конкурирующим читателем,
IdleEvent/join и закрытием fd прошла. На реальных ресурсах 8 music и 9 forms entries,
включая 2Open.gai (13406836 байт), потоковое чтение побайтово совпало с ReadPayload.
Максимальный наблюдавшийся C++ new при bounded streaming — 72112 байт;
это не измерение всех malloc библиотеки zlib и не замер общей RSS игры.

Это устраняет лишнюю полную копию, но не доказывает источник всех четырёх аппаратных OOM
и не подтверждает отсутствие аппаратной фрагментации/утечек.

Отдельный opt-in host fault вызывает bad_alloc в реальном музыкальном TFileStreamEC
после stage 8; перед продолжением driver ждёт фактического исключения worker.
Первая такая проверка прошла до RuinsTalk и ангара. Hook исключён из Switch сборки.

## Производительность и текст

Штатный host-маршрут аркады прошёл 60+ кадров. В последнем окне inter-present stage 45:
p50=17574, p95=19020, p99=20581 мкс. Stage 45 — интервал между представлениями;
stage 43 — SDL_RenderPresent на Switch. Это не сравнение до/после и не Switch FPS.
Измеримого нового ускорения аркады или генерации пока не заявляем.
Генератор, 300 ходов, RNG, физика и параметры мира не упрощались.

Длинный русский суффикс попыток заменён на краткий #N, проценты выводятся из счётчика
завершённой работы. Добавлено измерение оригинальным шрифтом до расширения label в
свободном месте до следующей колонки. Размер восстанавливается при закрытии экрана.
Проверка всех локализаций и визуальный Switch PASS остаются открыты.
Host-only capture сохраняет реальный RGB565 framebuffer в изолированный каталог прогона;
изображения и закрытые ресурсы в публичные CI артефакты не попадают.

## Публикация CI

Новый ci_stage_switch_artifacts.py проверяет Git SHA, clean flag, combined/preflight/link,
отсутствие undefined/excluded/stubs/startup-unimplemented, запрет аппаратной записи сейвов,
версии toolchain/pins, точные пути/размеры/SHA-256 и совпадение NRO module ID с GNU Build ID
matching ELF. Поддержаны PT_NOTE и настоящий Switch SHT_NOTE без PT_NOTE.
Повреждение/недостающие gates отклоняются до создания каталога публикации.
8 unit tests проходят; настоящий старый NRO/ELF также использован для проверки парсера.

Workflow публикует только на успешном push master после зависимых обязательных jobs:
NRO, matching ELF, build-report.json, artifact-provenance.json, SHA256SUMS.txt.
Локальный staging явно помечается CI UNVERIFIED.
**Реального нового GitHub run/download/hash verification пока нет.**
Новая clean NRO не выдаётся до закрытия программных gates.

## Открытые критерии

- P0.1: аппаратная первопричина, неподвижный курсор/потеря UI ввода после «Готово».
- P0.2/P0.3: аппаратная устойчивость; полный аудит live-pointer/object lifecycle.
- P1: сравнимые frame-time/генерация/world hash/RNG и батарея на Switch.
- Полная проверка подписи во всех локализациях и реальный UI screenshot.
- Clean combined NRO/ELF/provenance и подтверждённая загрузка из GitHub Actions.
- MatrixGame: ABI bridge, Switch GPU backend и возврат в кампанию. Windows x86
  baseline/настоящий бой/статистика проигрыша и native head fix test проверены ниже.
- Миграция исторических E2E имён без потери CFG/log compatibility.

## Последний host-прогон

`build/host-bounds-valid.json`, binary SHA-256
`63072984e909ede1b6675c67838c8992c7988e3f0d530976411d15872a9aa43f`.
С оригинальными PC-ресурсами и русской конфигурацией прошёл маршрут генерации →
«Далее» → RuinsTalk (20) → ангар (4), затем 60+ кадров игрового экрана.
Искусственная bad_alloc в настоящем музыкальном файловом worker после stage 8
зафиксирована до callback «Далее»; ошибка не остановила host UI.
35 проверок размеров подписей для допустимых состояний генератора: fits=1 во всех.
Негативные значения попыток проверены только для стадии, которая их производит.
Реальный host framebuffer сохранён в `build/intro-ready-russian.png` и просмотрен:
русские подписи процентов и стрелка видны. Это не аппаратный screenshot.
Процесс намеренно остановлен probe после проверки (stopped_by_probe=true),
crashed=false, timed_out=false; returncode=-9 не является самостоятельным crash.

Host breadcrumbs показывают перемещение игрового курсора, обработку down/up,
callback и запрос экрана 20. Уточнение пользователя: на Switch после «Готово»
замер весь интерфейс, включая курсор, поэтому навести его и нажать стрелку было
невозможно. Отсутствие пары событий внутри host-хитбокса не объясняет этот симптом.
Первопричина аппаратного зависания остаётся UNCONFIRMED.

Следующий необходимый результат: локализовать аппаратный отказ по доставке события
до и после GI callback и сопоставить его с видимым состоянием экрана.


## Продолжение: настоящий host touch и MatrixGame ABI

В `build/host-raw-touch.json` подтверждён отдельный маршрут: настоящие 300 ходов,
OOM музыкального worker, затем raw touch → PollHistory/нормализация → очередь →
GI down/up с touch=1 и press_id=1 → callback → RuinsTalk → ангар и 60+ кадров.
SHA-256 host binary: `bcff49500a23db74e60b2f00a24e808217fcecdd8e3a0717dad23248297c7ba1`.
Elapsed=60.0 с, stopped_by_probe=true, crashed=false, timed_out=false.
Готовые mouse-сообщения для «Далее» в этом режиме не инжектируются.

Исправлена гонка только host input injection: driver теперь публикует raw snapshot
под отдельным mutex, UI остаётся единственным владельцем состояния нормализатора.
Стресс-тест 10000 публикаций добавлен в существующий Win32 message test.
Тот же тест со старым `win32_compat_messages.cpp` из HEAD воспроизвёл TSan data race
между InjectHostRaw и PollHistory (`build/messages-before-tsan.log`).
Исправленный вариант: TSan и ASan/UBSan завершились с кодом 0.
ThreadSanitizer gate добавлен в существующий host-regression job.
Это не исправление аппаратного зависания: Switch читает физический HID другим путём.

MatrixGame: MSVC 19.44 x86 обнаружен и работает. Первоначальный отказ configure
из-за отсутствия DIRECTX9 устранён локальным извлечением официального Microsoft SDK;
установщик не запускался. Pinned DLL и EXE собраны, экспортная таблица DLL реально
загружена и проверена. Подробности и хеши: reports/matrixgame-20261018.md.
`tests/probe_matrixgame_abi_x86.cpp` скомпилирован настоящим MSVC x86 против pinned
`MatrixGameDll.hpp`, без подменённых деклараций: long/dword=4, wchar=2, pointer=4,
settings=56, statistics=24, robot interface=16, callbacks=64, text image=20 байт.
Проверены ключевые offsets, __cdecl export и __stdcall callbacks.
Init/Run/Support и бой этим probe не вызывались; ARM64 bridge ещё не реализован.
Ошибка head=MRT_WEAPON по-прежнему есть в pinned CConstructor.cpp:1479;
проверка реального выбора/оценки головы и адресная адаптация остаются открыты.

Для проверки ARM64 добавлен `--link-only`: полный compile/link/symbol/import audit
без упаковки запускаемой NRO. Такой отчёт не содержит matching NRO и не пригоден
для публикации через staging gate. Полная combined проверка завершилась PASS:
undefined_symbols=0, required_stubs=[], first_failure=null. ELF 56591528 байт,
SHA-256 `21682016267532fecdf00b5cc8b9bf6ffd13db9f8be742c2ce2e05c660be992d`.
Это локальный dirty ELF для аудита, не clean артефакт для аппаратного запуска.


## Английский host: удерживаемое касание

`build/host-english-bounds-ack.json`: binary SHA-256
`6001d7d578050b6e4ef9ecad85571392e6daee9d823d32ebce4f7745885fee59`,
elapsed=50.85 с, generation_complete=true, экраны 20 и 4, gameplay_60_frames=true,
stopped_by_probe=true, crashed=false, timed_out=false. Все 35 label-bounds fits=1.
Настоящий английский framebuffer `build/intro-ready-english.png` просмотрен:
подписи процентов и стрелка видны. Не заявляется проверка остальных локализаций.

Предыдущий английский прогон с фиксированным 150-мс касанием завершился timeout
(`build/host-english-bounds.json`): host driver отпустил raw snapshot до его чтения UI,
GI down/up отсутствовали. Драйвер исправлен: release только после наблюдаемого GI down,
с пределом ожидания 5 с. Успешный повтор проверяет удерживаемое касание; он не доказывает
доставку короткого тапа или аппаратную задержку ввода. Switch HID путь не изменён.

Инкрементальная ARM64 сборка под MSYS получила тот же обход некорректного CMake
compiler_depend.make с native Windows paths, который уже применялся под os.name=nt.
Проверка stamp manifest сохранила fail-closed поведение: первая combined сборка была
отклонена после обновления manifest во время компиляции, затем повтор с актуальным
stamp прошёл. Промежуточные NRO не упаковывались.


Обычный ARM64 путь также завершил полный compile/link/audit PASS:
undefined_symbols=0, required_stubs=[], first_failure=null, packaging=SKIPPED (--link-only).
Локальный отчёт сохранён в build/ordinary-arm64-link-report.json; генерируемый tracked
e2e-build-report.json восстановлен до HEAD, чтобы локальные пути/dirty данные не попали
в изменения. Все три pinned upstream checkout чисты. Diff whitespace check PASS.


## Диагностика завершения подготовки Introduction

После первоначальных host-прогонов добавлены отдельные отметки worker_release=begin/end
вокруг освобождения потока генерации и post_generation_pending=0 ready=1 после установки
таймера готового экрана. Они не меняют порядок расчётов или состояние кнопки.
BlinkContinueButton раз в 5 с пишет stage, наличие worker, MemorySnapshotActive,
ExitCode, active root/cursor, координаты игрового курсора, active/disabled кнопки,
число презентаций и последний sampled RGB565 hash. Presentation rate теперь также
указывает screen и этот hash; это хеш CPU framebuffer, не readback физического экрана.
Так можно отличить отсутствие завершения подготовки, неподвижный игровой UI и повторную
подачу одного буфера. Видимый результат SDL_RenderPresent требует аппаратной сверки.

Combined ARM64 повторный compile/link/audit с этой диагностикой PASS,
build/combined-ui-snapshot.log; effective manifest не изменился (254 imports,
45 dynamic calls, UNKNOWN=0). Python tooling regression: 25 тестов PASS;
staging: 8 PASS. Нового NRO нет.

Повторный host запуск новых снимков UI пока не выполнен: WSL дважды вернул
Wsl/Service/E_ACCESSDENIED до старта Python. Предыдущие результаты host маршрута
остаются результатами предыдущих хешей, а не доказательством исполнения новых снимков.

Текущий combined ELF SHA-256: `b8c1f830697afa8273a5476d21d7e01692e215709963899fcb0c3efad14b42c0`.


Обычный ARM64 путь с ui_snapshot также PASS, build/ordinary-ui-snapshot-final.log;
отчёт build/ordinary-ui-snapshot-report.json. Первая попытка выявила повторное создание
невалидных compiler_depend.make после простого обнуления. Исправление для MSYS теперь
преобразует Windows drive paths в исходных GCC *.obj.d в POSIX paths и удаляет
только производный compiler_depend.internal для повторного разбора. Сгенерированный
compiler_depend.make содержит реальные зависимости от project/SDK headers, а не пустой
список. На обычном пути проверено фактическое повторное создание этих правил и link/audit.
Ранее описанное простое обнуление кэша под MSYS заменено этой обработкой.


Повторная combined сборка с нормализацией GCC depfiles тоже PASS:
build/combined-ui-snapshot-deps.log. На обычном пути проверены 68 softfloat object rules,
наличие header dependency softfloat.h и отсутствие повреждённых drive-path prerequisites.


## Host с работающим heartbeat и обновление MatrixGame

Отказ WSL E_ACCESSDENIED устранён для проверки разрешённым запуском вне песочницы;
новая host-сборка успешно скомпилировала 2 изменённых unit и выполнила link.
build/host-ui-snapshot.json: SHA-256 a7a25e8fc4efc811a3a7774f3e4b60db52c26e9ca168836813a3abb0194bd50f,
elapsed=92.05 с, generation_complete=true, экраны 20/4, gameplay_60_frames=true,
crashed=false, timed_out=false. Реальный музыкальный worker fault произошёл между
worker_release=begin/end; далее post_generation_pending=0 и touch callback.

Добавлен opt-in E2E511_HOST_WAIT_UI_SNAPSHOTS: driver ожидает два ui_snapshot после
«Готово» перед обычным touch down/up. Он не меняет Switch и генератор.
build/host-ui-dwell.json: SHA-256 249f0e98b5d75c23c655d1833c9518916755412b47fe49b89d6ba1f0f3146d3e,
elapsed=83.57 с, stopped_by_probe=true, crashed=false, timed_out=false,
generation_complete=true, экраны 20/4, gameplay_60_frames=true.
Оба снимка: stage=8, worker_present=0, memory_snapshot=0, exit_code=0,
root_active=1, cursor_active=1, cursor=640,360, ok_active=1, ok_disabled=0.
Презентации 1435→1627, хеши 7e1bee6356b330df→67b9918c9c10162a, затем raw touch,
continue_callback_invoked=1 и настоящий переход. Heartbeat и изменяющийся framebuffer
подтверждены на host. Это не объясняет неподвижный аппаратный экран.

MatrixGame: настоящий Windows x86 бой с выбором/движением/атакой/гибелью роботов,
штатной сдачей и реальной статистикой 00:03:53 выполнен через computer-use.
Адресный head fix проверен настоящим CGame::Init/AI loader: исходная строка даёт
159 неверных типов и 23 mismatches компонентов цены/итогов; исправленная — нули.
Pinned upstream чист. Детали, ограничения и provenance: reports/matrixgame-20261018.md.
Native ARM64 bridge, Switch renderer и передача результата кампании остаются открыты.


Воспроизводимая native MatrixGame CMake-цель полностью собрана на свежем archive pin
с единственной gameplay заменой MRT_HEAD и успешно запущена: 159/10 шаблонов/голов,
0 ошибок типа/цены/итогов, exit=0. Код теста tests/probe_matrixgame_ai_templates_x86.cpp,
инструкция port/matrixgame/README.md. Не объявляется полный MG-1: победа,
UI-сборка модулей и возврат в кампанию пока не проверены.


## A6: настоящий serialized world и RNG old/new

Выполнены три candidate и три original-package control с фиксированными seed=314159,
именем и настройками. Все payload SHA-256 и 14 world/RNG checkpoints совпали.
Native OnSave меняет RNG, поэтому конечная сериализация — отдельная реальная операция
с завершением диагностического процесса; не названа read-only наблюдением.
Полный разбор, provenance и воспроизведение: generation-determinism-20261018.md.

Медиана bounded streaming 11.598790 с, original reader 9.465149 с в последовательной
host серии. Это наблюдаемое замедление, не улучшение; P1 performance gate остаётся
открыт. Memory benefit от удаления полной копии не превращается в утверждение о скорости.
Обычный/combined ARM64 link/audit PASS; executable bytes aGalaxy/fGameSettings
во всех 1348 секциях совпали с состоянием до host-only instrumentation.


Обычный host opt-out после добавления A6 helper тоже PASS:
build/host-after-determinism-normal.json (114.28 с, 300 ходов, музыка fault,
heartbeat, touch, RuinsTalk/ангар, 60+ кадров, без crash/timeout).
Seed override и диагностический snapshot в этом маршруте не выполнялись.


## Уточнение P0.1: предел старого аппаратного журнала

После continue-enabled в pack.zip записаны 140 окон presentation rate общей
длительностью 702059 мс и 723 input poll с 34 различными HID координатами.
Это подтверждает продолжение нижнего HID/present-loop; не движение видимого
игрового курсора. В этом интервале нет input callback diagnostics и CPU framebuffer
hash: старый журнал не различает блокировку GI dispatch, stale CPU framebuffer
и stale видимый output. Свидетельство пользователя о замершем экране сохраняется.
Данные: build/pack-post-ready-forensics.json. Случай не признан промахом по кнопке.


## Raw-cache budget: подтверждённый дополнительный дефект P0.2

TCBufEC учитывал 0 при реально выделенной ёмкости. Native before FAIL/fixed
ASan/UBSan PASS, normal host до ангара PASS, реальные 71 quest buffers теперь
учтены на 18250536 байт capacity. Actual recovery reclaim распознаёт освобождение
после Release; retained entry защищён. Три полных мира/RNG совпали до/после.
Текущий host SHA 5ce8a9af…07eb9f, ordinary/combined ARM64 link/audit PASS.
Подробности и границы: cache-buffer-accounting-20261018.md. Физическая причина
замершего экрана UNCONFIRMED, новая NRO и CI download proof ещё не готовы.


## Конкурентный recovery: подтверждённая гонка A3

Native ThreadSanitizer воспроизвёл несогласованные ResidentBytes reads в Reclaim
против producer write под CacheLock. Snapshot/trim теперь под одной существующей
блокировкой, I/O после unlock. Before TSan exit66; after 200 loads +2441 reclaim PASS.
Host SHA 11a5e0c9…3fb66e: normal до ангара, native ASan/UBSan, real FileStream faults,
три world/RNG сравнения и оба ARM64 link/audit PASS. Native regressions добавлены
в обязательный CI host-regression, remote execution пока нет. Подробности и границы:
reclaim-concurrency-20261018.md. Hardware freeze по-прежнему UNCONFIRMED.


Повторный A5 на текущем host SHA 11a5e0c9…3fb66e: native menu/arcade route
22 → 1 → 40 → 21, 60+ кадров, 40.76 с, crash=false, timeout=false.
Доказательство: build/host-reclaim-lock-arcade.json. Это функциональная host-проверка,
не физический FPS и не эквивалентность всей последовательности боевых состояний.
