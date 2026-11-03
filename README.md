# Space Rangers HD: A War Apart — Nintendo Switch

Неофициальный исходный homebrew-порт **Space Rangers HD: A War Apart** для Nintendo Switch (ARM64, libnx, SDL2; MatrixGame использует deko3d). Это проект переноса технических подсистем игры, а не готовый игровой релиз.

> **Статус на 02.11.2026: MatrixGame E610 — SOFTWARE PASS / HARDWARE PENDING.** Две актуальные standalone Quick Smoke и Interactive NRO собраны и проверены отдельно от основной игры. Аппаратные результаты старых версий не подтверждают текущую сборку. Для основной игры сохраняются ранее найденные ошибки фонов, ручного сохранения и зависания; проект не готов к полноценному прохождению или релизу.

## Последние результаты

- **Аркада:** пользователь отметил улучшение плавности. Host-оптимизация столкновений проверена на совпадение состояния, RNG и геометрии; дополнительный выигрыш на Switch не измерен. Пропуски диагностических записей не считаются пропущенными кадрами.
- **Сохранения:** в аппаратных данных остаются две ошибки `manual-publish ENOENT`; конкретные syscall и пути из старого журнала восстановить не удалось. Реальные host save/load с именем `Солнце`, отказами и восстановлением прошли. Добавлена диагностика фаз, errno и UTF-8 путей; аппаратное исправление не подтверждено. Формат порта — `RSG64/v168`; Windows `RSG/v167` отклоняется.
- **Повторяющиеся фоны:** текст генерации подтверждён на других экранах, включая планету. Установлен фактический ресурс `GI,Bm.FormRuins.2RCbg`, но владелец ошибочного растра в CPU-композиции ещё не найден. Адресные host-проверки ресурса дефект не воспроизвели; исправленным он не объявляется.
- **Зависания карты и выхода:** рассматриваются отдельно. Добавлены малозатратные thread/phase/progress markers и границы OSK; исправлен порядок остановки script worker перед освобождением данных. Host shutdown проверен, причина полного зависания консоли не доказана. Длительное сохранение может включать ожидание экранной клавиатуры.
- **OSK и GPU smoke:** результаты относятся к конкретным ревизиям. Данные старого основного порта и отдельных MatrixGame smoke не означают аппаратный PASS текущего E610; физический HID/GPU и поздние Init-исправления ожидают двух новых пользовательских запусков.
- **MatrixGame E591–E610:** полный оригинальный Init/Takt/Draw, естественное поражение и настоящий возврат результата в Gov проверены на Windows. Host sanitizers, ARM64/QEMU и devkitA64 compile/link подтвердили соответствующие программные компоненты; их границы и исторические неполные проверки сохранены в отчётах. Реализованы native providers, владение UI/script/campaign, окно/caller handoff и общий источник UI-ввода. Единая native Campaign/Gov entry и естественная победа ещё не подтверждены. MatrixGame не входит в основную NRO.
- **Артефакты:** основная NRO, matching ELF и оба аппаратных ZIP сохранены неизменными. Новые offline-кандидаты находятся отдельно; позднейшие исходники и коммиты не включены в старые комплекты автоматически.

Исторические срезы и данные основного порта: [MatrixGame: оригинальный Init и прежняя граница](reports/matrixgame-e2e519-original-init.md),
[аппаратный анализ 23 октября](reports/e2e5/hardware-20261023-analysis.md),
[офлайн-стабилизация и открытые причины](reports/e2e5/hardware-20261023-stabilization.md),
[MatrixGame: актуальная граница bootstrap](reports/matrixgame-e2e519-engine-bootstrap.md),
[офлайн-аудит E2E-5.16.1](reports/e2e5/e2e516-offline-stabilization.md),
[паспорт неизменённого комплекта](reports/e2e5/e2e516-artifact-provenance.json).

### Подготовленный аппаратный комплект E2E-5.16

Исходный Git SHA: `03651ced28ecf0125f0b62c4f8129bd71e27f5c8`.
Пакет подготовлен локально в `build/e2e516/hardware-e2e516-03651ce.zip`;
этот путь не является ссылкой на опубликованный GitHub Release.
Именно этот комплект использован в аппаратном прогоне 23.10.2026;
его результаты и открытые дефекты приведены выше.

| Файл в комплекте | SHA-256 |
| --- | --- |
| `SpaceRangersHD-perf516.nro` | `9f654daab36c71cb51940871d4a93bdcceb09edd7da8dde127ab023bfb21e789` |
| `SpaceRangersHD-perf516.elf` | `0ca47fa95e614167da814cf63e1bc19e0bd005e94b34763287d713d5c9989ab5` |
| `hardware-e2e516-03651ce.zip` | `e1a1dec5f410b983d629ae538ad9b4ba063f89a8101af43d27ea00c6ce6a08a4` |

Build ID: `b6c18a13cef88fe757d683e18cf48fc667067bda`. Release ARM64:
сохранения и звук включены, trace выключен, perf-диагностика включена.
`build_dirty=true` вызван пользовательскими untracked-файлами, не вошедшими
в сборку; скомпилированные tracked inputs соответствуют указанной ревизии.
Позднейшие коммиты отчётов и MatrixGame не меняют идентичность этого комплекта.

При следующем доступе к консоли приоритетны три короткие проверки:
генерация → станция → карта → меню с диагностикой сцен и фото дефекта;
пропуск дня и приказ перемещения на копии готового сейва;
сохранение после перелёта с Unicode-именем и загрузка после перезапуска.
Инструкция сбора логов и оставшиеся gates приведены в отчёте E2E-5.16.

## MatrixGame E610 Freeze: два запуска

**Результат r5:** прежний binding guard пройден, но оба запуска завершились
SDK fatal break в material uniform MemBlock при DrawLandscape миникарты.
DkResult_Fail не означает доказанный OutOfMemory. Обнаружено расхождение
metadata deko3dd и фактического libdeko3d.a; прежний gate его не проверял.
[Разбор matching ELF/crash reports](reports/matrixgame-e610-r5-hardware-20261102.md).

**r6:** единый SDK selector и проверка фактических archive objects по map;
один material uniform GPU block вместо двух; до 64 живых draw packets с release
после подтверждённого completion. Все draw calls сохраняются. CPU packet bytes,
порядок и lifetime прошли адресные Windows/host sanitizer/ARM64 QEMU проверки.
[Инструкция r6](docs/matrixgame-e610-material-r6-readme.ru.md).
Аппаратное прохождение этого варианта ещё не подтверждено.

**Результат r4:** оба аппаратных запуска выполнили по 474 managed GPU evictions
и дошли до CMinimap::RenderBackground. Новый отказ — staging vertex binding;
полный Init/игровые кадры ещё не прошли.
[Аппаратное evidence r4](reports/matrixgame-e610-r4-hardware-20261102.md).

**r5:** native vertex/index binding теперь выполняет настоящий setter
удерживаемого staging Context, затем проверяет результат. Ограничение stream 0,
проверки владельца, locked/range/stride и snapshot lifetime сохранены.
Декодирование для NativeDraw читает байты нового выбранного буфера.
[Инструкция r5](docs/matrixgame-e610-staging-r5-readme.ru.md).
Аппаратное прохождение Init этим вариантом ещё не подтверждено.

Проверенный пакет r5: `dist/matrixgame-e610-switch-test-r5-staging.zip`.
Source SHA `937eccd13fc2b8911863459ae6e0ee734c31adca`;
ZIP SHA256 `f4daff97e481550fa0bff3bd5b8cd78053963c4ebd9a90d9926fb69407f8da68`.
Windows/host sanitizer/ARM64 QEMU и SDK/package checks — PASS;
аппаратный r5 — PENDING. [Отчёт и SHA r5](reports/matrixgame-e610-staging-r5.md).

**Результат r3:** аппаратные Quick/Interactive прошли VI query, ApplyVideoParams
и побайтовое сравнение identity presentation. Поздний Init остановился на
`original managed texture decode failed: -1`; полные игровые кадры не достигнуты.
[Аппаратный отчёт r3](reports/matrixgame-e610-r3-hardware-20261102.md).

**r4:** добавлено безопасное вытеснение managed GPU images при исчерпании
фиксированной арены; CPU mip data и handles сохраняются, изображения,
удерживаемые записанными draw/surface/caller references, не вытесняются.
Учитываются priority и последнее использование. Ошибки загрузки сохраняют
имя ресурса, размер входа и исходный код вместо общего -1.
Аппаратная причина r3 не устанавливается задним числом: там исходный код
и ресурс не были записаны. Новый вариант требует пользовательской проверки.

Финальный пакет r4: `dist/matrixgame-e610-switch-test-r4-managed-final.zip`.
Source SHA `d5af0be47d5d48b8d8f392c403272cb6f9486026`;
ZIP SHA256 `6cd564d1706a82b6940313efdaaa608ed240d5b57e5591059050fe2cf8d3f945`.
Адресные Windows/host sanitizer/ARM64 QEMU проверки и SDK link/package — PASS;
аппаратный запуск r4 — PENDING.
[Проверки и SHA r4](reports/matrixgame-e610-managed-r4.md),
[инструкция r4](docs/matrixgame-e610-managed-r4-readme.ru.md).

**Аппаратный результат r2:** два Quick и один Interactive подтвердили BC1/gamma
smoke и PrepareMap, затем остановились в ApplyVideoParams:
`viOpenDefaultDisplay failed: 4722`. Полный Init и кадры карты не прошли.
[Анализ журналов и фото](reports/matrixgame-e610-r2-hardware-20261102.md).

**r3:** исправлены повторное открытие VI display и ориентация gamma-прохода;
добавлена полнокадровая проверка identity presentation. Один явный владелец
создаёт VI display/layer/window и освобождает их в обратном порядке, а mode query
переиспользует живой display. Имена в hbmenu теперь различаются явно:
`MatrixGame r3 Quick Smoke` / `MatrixGame r3 Interactive`.
[Инструкция r3](docs/matrixgame-e610-display-r3-readme.ru.md).
Аппаратное подтверждение r3 ещё не получено; исторические пакеты ниже сохранены.

Проверенный пакет r3: `dist/matrixgame-e610-switch-test-r3-display.zip`.
Source SHA `d09b7859e693bc6d33d0c8328ecd5bbc50839c52`;
ZIP SHA256 `e0cd5e91e6a241be8ca3fcd92b24eff90d05334833c0982da74dbe5e71863632`.
[Проверки и контрольные суммы r3](reports/matrixgame-e610-display-r3.md).

**Уточнение r2:** для уже скопированного DATA подготовлен вариант с Rangers root
`sdmc:/switch/space-rangers-hd-a-war-apart/game`; повторная копия всех 2+ ГБ
в `matrixgame-diag/rangers` не нужна. [Инструкция r2](docs/matrixgame-e610-shared-data-readme.ru.md).
Отдельный MatrixGame root с robots.pkg и двумя текстовыми CFG сохранён.
Пользователь наблюдал перевёрнутый по обеим осям текст FAIL в прежних двух NRO;
причина FAIL и ориентация ожидают анализа run.jsonl. Исправление графики не заявляется.
Старый E610 ZIP сохраняет свои исходники, пути и SHA, указанные ниже.

Пакет r2: `dist/matrixgame-e610-switch-test-r2-shared-data.zip`;
SHA256 `1925e0942bfa77a157720424bef3b4d0dedde29b7d88fc909a0d74741f313ad2`.
Исходники r2: `8c3612d2c347ffcc26d76bf12632f5d2d58d78b0`.
[Результат сборки r2 и аппаратное наблюдение](reports/matrixgame-e610-shared-data-r2.md).

Текущий программный этап заморожен перед аппаратной проверкой. Новую функциональность
Campaign/Gov для этого пакета не добавляем. Все результаты на Switch до новых
пользовательских наблюдений — **HARDWARE PENDING**.

- **Quick Smoke:** существующая standalone entry, настоящий native/deko3d Init,
  карта, четыре завершённых видимых кадра карты, cleanup. Загрузочные кадры и
  синтетический smoke не подменяют карту.
- **Interactive:** тот же Init и до 120 секунд ручного original native loop после
  Init; курсор/Joy-Con, меню/пауза, камера и доступные робот/звук/стрельба.
  Штатный запрос выхода — **Minus+Plus**.
- Это standalone-проверки. Они не создают RangersNativeInput/UiContext/Campaign
  owners и не вызывают Galaxy/Gov → Robot::FRun; присутствие компонента в dependency
  archive или ELF не доказывает его вызов.

Локальный пакет: `dist/matrixgame-e610-switch-test.zip`. Это не опубликованный
GitHub Release; исходники и отчёты находятся в контрольной ветке. SHA исходников,
обеих NRO и build ID приводятся в manifest.json пакета и [итоговом freeze-отчёте](reports/matrixgame-e610-switch-freeze.md).

Исходники сборки: `6f1df1628984c8c0ac49549e8a7bf9d0aab10a4a`,
ветка `codex/matrixgame-e610-freeze-20261102`.
Build ID исходников: `6f1df1628984-7b76555a72b8e4dda20c`.
Последующий коммит README и итоговых отчётов не меняет входы этой сборки.

| Файл | SHA256 |
| --- | --- |
| `matrixgame-e610-quick-smoke.nro` | `ad5b2f074787e97199efa2b0ba56554dda6a929db9e2ad406459f515d4b6e2bd` |
| `matrixgame-e610-interactive.nro` | `f0f30407c0b2516696b01aefbab31039dfa8e13f758e640cd6e0d3f69402484a` |
| `matrixgame-e610-switch-test.zip` | `edf634dce3a4e722a0a2819b560b45aa84885607676eb5b9eb9294201f818e21` |

[Инструкция: копирование, ресурсы, управление, два запуска и сбор журналов](docs/matrixgame-e610-switch-test-readme.ru.md).
[Точная программная граница E610](reports/matrixgame-e2e610-native-ui-pump.md).
[Состояние Init](docs/matrixgame-init-sequence.md).

Воспроизведение на подготовленном локальном SDK/dependency state (создаёт новую
папку сборки, не меняет основную NRO и старые тесты):

```powershell
python tools/matrixgame_current_diagnostic_sdk.py --output-root build/matrixgame/e610-switch-freeze
```

Проверенные dependency manifests и существующие оригинальные SDK archives нужны
локально; одна загрузка исходников не заменяет эту подготовку. Точные входные
хеши и configure/build команды сохраняются с каждой сборкой. Игровые ресурсы,
пользовательские сохранения, сырые captures/logs и бинарные dist-пакеты в Git
не включаются. После подготовки пакета разработка остановлена до результатов
двух аппаратных запусков.

## Что уже работает

Ниже зафиксированы проверки соответствующих ревизий. Текущие аппаратные дефекты
и границы более поздних изменений перечислены в сводке выше.

| Область | Состояние |
| --- | --- |
| Сборка ARM64/NRO | Локальный Release Save ON комплект E2E-5.16 проверен по ELF, manifest и хэшам; актуальный Switch PASS ожидается |
| CI `package-host` | Результат конкретной ревизии проверяйте в [Actions](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/workflows/package-host.yml); локальная проверка E2E-5.16.1 не означает CI PASS |
| Иконка, NACP и метаданные NRO | PASS — имя, JPEG в ASET |
| Package filesystem и `common.pkg` | host PASS, ARM64 build PASS |
| Переведённый `EC_File` | host PASS, ARM64 build PASS |
| Загрузка INSTALL.TXT, language/mod packages | host/CI PASS, ARM64 build PASS |
| CPU RGB565 / SDL presentation | host/CI PASS, ARM64 build PASS |
| DAT/runtime configuration, GlobalCache и M12 persistent loop | host/CI PASS, ARM64 build PASS, hardware PASS |
| M13 bitmap metadata diagnostic | host/CI PASS, ARM64 build PASS, hardware PASS (17/17) |
| M14P portable GI Format-0 CPU decode | host/CI/ARM64/hardware PASS; M14 COMPLETE |
| M15 portable GAI container + one Format-0 frame | release baseline + CI + ARM64 build + Switch hardware PASS; M15 COMPLETE |
| M16 portable GAI Format-2 decode | host/CI/ARM64/Switch hardware PASS; M16 COMPLETE |
| M17 portable GAI sequence playback | host/CI/ARM64/Switch hardware PASS; one `Flags == 0` cycle matches oracle; M17 COMPLETE |
| M18 portable software compositor | host/CI/ARM64/Switch hardware PASS; M18 COMPLETE |
| M19 portable scene compositor | host/CI/ARM64/Switch hardware/Python oracle PASS; M19 COMPLETE |
| M20 portable GI object layer | host/CI/ARM64/Switch hardware PASS; M20 COMPLETE |
| M21 portable UI image foundation | host/CI/ARM64/Switch hardware PASS; real Simple oracle matches; Trans/Alpha release baselines are genuinely NOT PRESENT; M21 COMPLETE |
| M22 portable UI object/layout foundation | host mixed-tree/Python oracle, CI `37783935697`, clean ARM64, symbol audit и Switch hardware PASS; M22 COMPLETE |
| M23 portable AFT/text/Label foundation | host/Python oracle, CI `37803515979`, clean ARM64/symbol audit PASS; cumulative M24 Switch log: checkpoint и dynamic stage PASS; M23 COMPLETE |
| M24 portable GraphButton/Window/Zone foundation | host/Python oracle, CI `37823081354` и `37825116392`, clean ARM64/symbol audit PASS; cumulative Switch log: checkpoint, dynamic stage, PLUS и shutdown PASS; M24 COMPLETE |
| M25 portable GI/GAI UI and real release subtree | host/CI/ARM64/Switch PASS; 18-node PLBar and live GAI; M25 COMPLETE |
| M26 portable GraphBuf, ScrollBar, PanelScrollBar, Edit and 50-node real UI | host/oracles, CI, ARM64 и накопительный Switch-прогон `f9c98de` PASS; M26 COMPLETE |
| M27 portable input, focus and event dispatch | host/oracles, CI, ARM64 и накопительный Switch-прогон `f9c98de` PASS; M27 COMPLETE |
| E2E-5: меню, Options и New Game | аппаратная `e959e85` прошла меню, Options, выбор персонажа, генерацию и вход в игру; сохранение русского языка требует повторной проверки после локального исправления Save и перезапуска |
| E2E-5: игровая навигация | пользователь открыл ангар, карту и магазин; низкий FPS и рывки курсора остаются; host также проверил оригинальный `ProgramMain` и базовый New Game |
| E2E-5: вылет из ангара | на Switch загрузка остановилась на 85% с `FAIL stage=gai key=Bm.Hits.05`; общий GAI-путь исправлен локально, а новый host-прогон прошёл этот ресурс и 60 циклов StarMap; аппаратная проверка ожидается |
| Оригинальный save/load | HOST: ручной слот и quicksave реально записаны, загружены в новых процессах; после пяти ходов новое сохранение повторно загружено. Файлы `RSG/v167` отклоняются. SWITCH: round-trip ожидается |

Аппаратные проверки — отдельный gate: результаты host, CI и кросс-сборки не считаются доказательством работоспособности на консоли.

## Baseline и границы проекта

Эталон — установленный Steam/GOG build от **2025-10-13**, версия игры `2.1.2500`:

```text
Rangers.exe SHA-256
83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98
```

В репозиторий не входят игровые ресурсы, оригинальные бинарные файлы и коммерческий контент. Для запуска нужна собственная легально установленная копия игры. Не публикуйте эти файлы вместе с NRO или в issues.

## Получение исходников

Подмодули обязательны:

```powershell
git clone --recursive https://github.com/sklart/space_rangers_hd_a_war_apart_port.git
cd space_rangers_hd_a_war_apart_port
```

Если checkout уже существует:

```powershell
git submodule update --init --recursive
```

Исходники движка планетарных боёв подключены отдельным подмодулем
[`upstream/matrixgame`](upstream/matrixgame) из [vladislavrv/MatrixGame](https://github.com/vladislavrv/MatrixGame).
Зафиксирована ревизия `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`.
Полный движок не включён в основную NRO. Независимые native CPU-компоненты
и отдельный deko3d smoke launcher не подтверждают работоспособность боя.
Состояние описано в [MatrixGame README](port/matrixgame/README.md),
план — в [MatrixGame Switch plan](docs/matrixgame-switch-plan.md).

## Требования к сборке

- devkitPro MSYS2;
- devkitA64 и libnx;
- Switch portlibs: SDL2, JPEG, PNG и zlib;
- CMake и Python 3;
- Git.

Linux host-регрессии выполняет GitHub Actions `ubuntu-24.04`. Локальный WSL используется для дополнительных проверок исходных методов на x86 и ARM64/QEMU; они не заменяют полный CI и запуск на Switch. Для основной ARM64 NRO используется devkitPro MSYS. Отдельные MatrixGame CMake targets также проверяются Windows devkitA64 через указанный выше Python builder; их provenance сохраняется отдельно.

Основную сборку через Makefile ниже запускайте из оболочки devkitPro MSYS. Это отдельный путь от диагностических MatrixGame CMake targets.

```bash
cd port/switch
make clean
make
```

Результат ровно один:

```text
port/switch/Space Rangers HD - A War Apart.nro
```

`make` перед созданием NRO проверяет `assets/icon.jpg`: это должен быть RGB JPEG размером ровно 256×256 и не больше 128 KiB. Метаданные NRO: имя `Space Rangers HD: A War Apart`, автор `sklart`, версия `0.0.1`.

## Структура SD-карты

```text
<SD>/switch/space-rangers-hd-a-war-apart/
  Space Rangers HD - A War Apart.nro
  game/                    # ваша полная установленная копия игры
    Rangers.exe
    DATA/
    CFG/
    ...
  config/                  # создаётся портом
  save/                    # создаётся портом
  logs/
    port.log               # диагностический лог порта
    port-prev.log          # предыдущий диагностический запуск
    gr-main.log            # отдельный лог GR_Main
    e2e.log                # этапы оригинального E2E-5; дополняется при запусках
  runtime/
    deployment.txt
```

Для первого теста копируйте установленную игру целиком в `game/`; не заменяйте и не удаляйте отдельные папки выборочно. При последующих обновлениях заменяйте только NRO, сохраняя `game/`, `config/`, `save/` и `logs/`.

## Безопасное развёртывание

`tools/deploy-switch.ps1` проверяет baseline по хэшу `Rangers.exe`, не использует `/MIR`, `/PURGE` или `/MOVE` и не трогает дерево игры при режиме обновления.

Первое копирование на SD-карту:

```powershell
.\tools\deploy-switch.ps1 `
  -SdRoot 'E:\' `
  -NroPath '.\port\switch\Space Rangers HD - A War Apart.nro' `
  -GameSource 'D:\Games\Space Rangers HD A War Apart' `
  -InitialGameCopy
```

Следующая сборка:

```powershell
.\tools\deploy-switch.ps1 `
  -SdRoot 'E:\' `
  -NroPath '.\port\switch\Space Rangers HD - A War Apart.nro' `
  -UpdateOnly
```

Сбор базовых журналов без изменения SD-карты:

```powershell
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -CollectLogs
```

При подключении по DBI/MTP/Wi-Fi скопируйте NRO и каталог `game/` в ту же структуру вручную. После каждого теста сохраняйте `port.log`, `gr-main.log` и `e2e.log`, а при аварии — новый отчёт Atmosphère. Текущий `-CollectLogs` собирает только `port.log`, `port-prev.log` и `gr-main.log`, поэтому `e2e.log` нужно скопировать отдельно. Код E2E открывает его в режиме добавления: удалять файл перед запуском не обязательно, но отдельная копия после каждого теста помогает точно сопоставить попытки.

## История подтверждённого аппаратного пути

Ниже сохранены результаты прежних milestone и сборок. Актуальные открытые
дефекты и состав подготовленного комплекта указаны в начале README.

Ранние переносимые подсистемы M14P–M25 прошли свои аппаратные проверки; это отдельные milestone, а не подтверждение полного игрового сценария. Аппаратная combined-сборка `e959e85` уже проходит прежнее место аварии RuinsTalk, открывает игровое меню, ангар, карту и магазин. При сохранении настроек в Options журнал фиксировал `Exception UnitGet Type=Resolution`; локально исправлен поиск фиксированного режима экрана, применение VSync и повторная инициализация рендерера после штатного перезапуска. Host-тест сохранил русский язык, громкость и переключение `Music=False`, автоматически перезапустил игру и повторно открыл Options. Аппаратное подтверждение этих новых исправлений ещё требуется.

По последним архивам `1.zip` и `2.zip` музыка на Switch искажается и прерывается при переходах и 300-ходовой генерации, а программная отрисовка меню занимает 60–143 мс. Для звука локально подготовлены увеличение SDL-блока до 2048 кадров, повышение приоритета музыкального потока и сокращение вычислений в callback. В общем UI-цикле локально исправлена очередь областей: начальный полный viewport больше не отрисовывается бесконечно; при одинаковом CFG host-площадь отрисовки снизилась примерно с 179 до 85 млн пикселей за сопоставимое окно. Для первого кадра Options подготовлено пиксельно идентичное быстрое масштабирование: host-время операции снизилось с 2,15 до 0,064 с. Host-профиль генерации сузил основное время до ежедневного обновления кораблей, но поведение игры ради ускорения не изменялось. Вылет из ангара выявил конечный пустой элемент последовательности `Bm.Hits.05`; исправление общего GAI-пути прошло проверку реального ресурса с ASan/UBSan и новый host-прогон до 60 циклов StarMap. Плавность курсора, слышимое качество музыки и скорость работы на Switch остаются открытыми. Подробная [сводка всех 11 аппаратных замечаний](reports/e2e5/hardware-e959e85-followup.md) отделяет причины, локальные проверки и оставшуюся работу.

Предыдущие host-доказательства сохранены: [отчёт о диалоге и ангаре](reports/e2e5/host-dialog-hangar.json), [аудиоотчёт](reports/e2e5/host-audio-capture.json) с корреляцией PCM 0,995 и [матрица покрытия](docs/e2e5-coverage-matrix.md). Исторические проверки сохранений описаны в [SAVE-06](reports/e2e5/e2e512-save06-corrupt-host.md), [SAVE-07/08](reports/e2e5/e2e512-save07-08-transaction-host.md), [SAVE-09](reports/e2e5/e2e512-save09-five-turn-host.md) и [SAVE-10](reports/e2e5/e2e512-save10-windows-format-host.md). [Сценарий E2E-5.12](docs/testing/switch-e2e512.md) относится к прежнему этапу. Существующий `Switch-E2E5-TEST.nro` в корне репозитория **устарел и не предназначен для повторного теста**; актуальный подготовленный комплект E2E-5.16 указан выше, всегда сверяйте NRO/ELF и save-mode.

```text
[M9] FAIL runtime config=... DAT zlib decompression failed
```

Это диагностический отказ, а не успешный игровой старт. Если экран перестал отвечать либо Home не открывает системное меню, удерживайте кнопку питания для безопасного перезапуска и приложите оба лога к отчёту.

## Проверки

GitHub Actions workflow `package-host` выполняет asset-free host-регрессии, включая corrupt package, циклы каталогов, `ZL02`, portable `EC_File`, synthetic DAT/runtime settings, M8 golden, M12, M13, M14P Format-0, M15 GAI container, M16 Format-2, M17 pure playback, M18 software compositor и M19 scene compositor. M18/M19 symbol audit запрещает графические/upstream GAI runtime типы в чистых CPU-композиторных binary.

M20 добавляет `host-gi-object-test` и отдельный symbol audit: он покрывает resource → GIObject → animation → Scene → RGB565 framebuffer и запрещает возврат к UI/Direct3D/`TGraphBufGR`-пути в новом object layer.

Для E2E-5 доступны проверка указателей, запуск host-проверок с собственной установленной игрой и проверка исходных методов GI/кэша на Linux или WSL:

```text
python tools/e2e5_static_audit.py --check --output reports/e2e5/pointer-width-audit.json
python tools/e2e5_host_runner.py --game-root "<каталог установленной игры>"
bash tools/e2e5_original_graphbuf_host.sh "<каталог установленной игры>"
bash tools/e2e5_original_graphbuf_arm64_host.sh "<каталог установленной игры>"
python tools/e2e5_report.py --output reports/e2e5/summary.json
```

Для ARM64/QEMU нужны `aarch64-linux-gnu` GCC/G++, QEMU user mode, CMake и заголовки JPEG/PNG. Уровни проверок, зависимости и ограничения описаны в [архитектуре тестов E2E-5](docs/e2e5-test-architecture.md). Отчёт может показывать `PARTIAL` или `NOT RUN` при успешном завершении отдельных команд: это не результат полного игрового сценария.

Локальная ARM64-сборка проверяет создание NRO. Дополнительно для финального артефакта следует подтвердить:

- `ELF64 AArch64` у `build/SpaceRangersHDAWarApart.elf`;
- наличие NRO;
- наличие ASET и совпадение embedded JPEG с `assets/icon.jpg`;
- SHA-256 перед копированием на SD-карту.

## Документация

- [Текущий статус и границы milestone](docs/current-status.md)
- [Сборка Switch](docs/build-switch.md)
- [Развёртывание на SD-карту](docs/switch-deployment.md)
- [Milestone 9: runtime configuration](docs/milestone9-runtime-config.md)
- [Milestone 10: build и hardware baseline](docs/milestone10-build.md)
- [Milestone 11: GlobalCache diagnostic slice](docs/milestone11-global-cache.md)
- [Цепочка зависимостей startup](docs/startup-dependency-chain.md)
- [M14 portability boundary](docs/milestone14-gi-portability.md)
- [M14P portable GI Format-0](docs/milestone14p-gi-format0.md)
- [M15 portable GAI CPU](docs/milestone15-gai-cpu.md)
- [M16 portable GI Format-2 CPU](docs/milestone16-gi-format2.md)
- [M17 portable GAI playback](docs/milestone17-gai-playback.md)
- [M18 portable software compositor](docs/milestone18-software-compositor.md)
- [M19 portable scene compositor](docs/milestone19-scene-compositor.md)
- [M20 portable GI object layer](docs/milestone20-gi-object-layer.md)
- [M21 portable UI image foundation](docs/milestone21-ui-image-foundation.md) — COMPLETE
- [M22 portable UI object/layout foundation](docs/milestone22-ui-object-layout.md) — COMPLETE
- [M23 portable AFT/text/Label foundation](docs/milestone23-font-label.md) — COMPLETE / HARDWARE PASS in cumulative M24 run
- [M24 portable GraphButton/Window/Zone foundation](docs/milestone24-ui-controls.md) — COMPLETE / HARDWARE PASS
- [M25 portable GI/GAI UI and first real subtree](docs/milestone25-gi-gai-real-ui.md) — COMPLETE / HARDWARE PASS
- [M26 GraphBuf, scroll controls, Edit and larger real UI](docs/milestone26-graphbuf-scroll-edit-real-ui.md) — COMPLETE / HARDWARE PASS
- [M27 portable input, focus and event dispatch](docs/milestone27-input-focus-events.md) — COMPLETE / HARDWARE PASS
- [E2E-4: безопасная экспериментальная ротация quicksave и ручное восстановление](docs/e2e4-save-load.md)
- [E2E-5: архитектура тестов](docs/e2e5-test-architecture.md)
- [E2E-5: матрица покрытия](docs/e2e5-coverage-matrix.md)
- [E2E-5: обнаруженные дефекты](docs/e2e5-found-defects.md)
- [E2E-5: измерение производительности](docs/e2e5-performance.md)
- [E2E-5: сводка 11 замечаний по аппаратной combined-сборке e959e85](reports/e2e5/hardware-e959e85-followup.md)
- [E2E-5: план одного аппаратного теста](docs/e2e5-hardware-plan.md)

## Лицензирование и обратная связь

Код порта распространяется в рамках лицензий данного репозитория и его подмодулей. Торговые марки, игровой контент и оригинальные assets принадлежат соответствующим правообладателям.

В отчёте об ошибке укажите commit, SHA-256 NRO, способ развёртывания, структуру папки на SD (без игровых файлов) и оба лога. Не прикладывайте коммерческие assets.

### MatrixGame E610 r6: проверенный тестовый пакет

Предыдущий отдельный пакет r6: `dist/matrixgame-e610-switch-test-r6-material.zip`; source `7036820f4c818fb8fce4fb94f6a98a7a822a5142`, build `7036820f4c81-2be9a052b1f566893f12`.

Две NRO называются в hbmenu **MatrixGame r6 Quick Smoke** и **MatrixGame r6 Interactive**. SDK/CPU/package checks: SOFTWARE PASS; аппаратное исполнение: HARDWARE PENDING.

[Инструкция двух запусков](docs/matrixgame-e610-material-r6-readme.ru.md) · [SHA256 и результаты проверок](reports/matrixgame-e610-material-r6.md) · [Evidence](reports/matrixgame-e610-material-r6-evidence.json).

### MatrixGame E610 r7: граница Init → Draw

Два аппаратных запуска r6 завершили настоящий Init, но отказали на диагностической CaptureState активного кадра. r7 завершает pending GPU commands и возвращает служебный image перед snapshot/loop; guard сохранён. [Аппаратный разбор r6](reports/matrixgame-e610-r6-hardware-20261103.md), [изменение и проверки r7](reports/matrixgame-e610-init-r7.md), [инструкция](docs/matrixgame-e610-init-r7-readme.ru.md). Новая сборка HARDWARE PENDING.

Предыдущий пакет r7: `dist/matrixgame-e610-switch-test-r7-init.zip`; source `9c097a23021adc11692bf634d9b0f793245b9281`, build `9c097a23021a-4294303d9d29695f9fa3`. Две NRO: **MatrixGame r7 Quick Smoke** и **MatrixGame r7 Interactive**. Контрольные суммы и результаты — в [отчёте r7](reports/matrixgame-e610-init-r7.md); SOFTWARE PASS / HARDWARE PENDING.

Аппаратная проверка r7 (03.11.2026): оба запуска завершили Init и показали оригинальный экран, но transition не завершился до deadline. Interactive: 48 transition frames / 121.790 с, около 0.394 кадра/с. **HARDWARE PARTIAL / PERFORMANCE FAIL**, полный smoke не пройден. Музыка предположительно слышна по сообщению пользователя; HID/бой не приняты. [Разбор и границы evidence](reports/matrixgame-e610-r7-hardware-20261103.md).

### MatrixGame E610 r8: оптимизация draw allocations и telemetry

Bounded reuse завершённых SDK blocks; порядок draw, completion guards и критерии smoke сохранены. На host journal replay медиана 14.44 → 1.31 с, 7747 → 671 records с сохранённым fsync. CPU-model pool: 417616 block allocations → 128 initial + 417488 reuse hits на 92 сохранённых frame counts. Эти числа не являются Switch FPS. [Изменения/измерения](reports/matrixgame-e610-perf-r8.md), [инструкция](docs/matrixgame-e610-perf-r8-readme.ru.md). R8 HARDWARE PENDING.

Предыдущий пакет r8: `dist/matrixgame-e610-switch-test-r8-perf.zip`; source `470184466ed857bd1e67b2072ce0ead6d6f22356`, build `470184466ed8-2a6f525547eec077fa9f`. В hbmenu **MatrixGame r8 Quick Smoke** / **MatrixGame r8 Interactive**. [SHA256 и evidence](reports/matrixgame-e610-perf-r8.md). SOFTWARE PASS / HARDWARE PENDING.

### MatrixGame E610 r9: оптимизированная native сборка

[Новые аппаратные журналы r8](reports/matrixgame-e610-r8-hardware-20261103.md) подтвердили улучшение Draw 2.53 → 1.61 с, но transition timeout сохранился. Native compile commands оказались без optimization. R9 включает **-O2 с assertions, exceptions и debug deko3dd**, проверяет реальные compile commands и fingerprints compiler profile. [CPU сравнение и проверки](reports/matrixgame-e610-opt-r9.md), [инструкция](docs/matrixgame-e610-opt-r9-readme.ru.md). HARDWARE PENDING; host speedup не равен Switch FPS.

Предыдущий пакет r9: `dist/matrixgame-e610-switch-test-r9-opt.zip`; source `161b6d5a0923969af93762a252f2477db98cdb60`, build `161b6d5a0923-cac3d1dc698dae229e1a`. В hbmenu **MatrixGame r9 Quick Smoke** / **MatrixGame r9 Interactive**. [SHA256 и evidence](reports/matrixgame-e610-opt-r9.md). SOFTWARE PASS / HARDWARE PENDING.

### MatrixGame E610 r10: переход на native разрешении

[Аппаратный r9](reports/matrixgame-e610-r9-hardware-20261103.md): Draw улучшился до 0.56 с, Init до 28.5 с, но transition timeout сохранился. В ожидаемом CFG нет geometry layout 1280x720. R10 масштабирует существующую authored geometry и скорость, сохраняя original Takt/capture/render; forced skip не добавлен. [Изменение и CPU oracle](reports/matrixgame-e610-transition-r10.md), [инструкция](docs/matrixgame-e610-transition-r10-readme.ru.md). HARDWARE PENDING.

Предыдущий пакет r10: `dist/matrixgame-e610-switch-test-r10-transition.zip`; source `dd7f6714872d1826bc35f896ee4975bc104f9ea8`, build `dd7f6714872d-507a9538a658856b38ba`. В hbmenu **MatrixGame r10 Quick Smoke** / **MatrixGame r10 Interactive**. [SHA256 и evidence](reports/matrixgame-e610-transition-r10.md). SOFTWARE PASS / HARDWARE PENDING.

### MatrixGame E610 r11: reuse upload blocks без Switch

Bounded best-fit и cached immutable size/CPU/GPU addresses уменьшают SDK queries/allocations без изменения draw payload или completion ownership. Physical capacity входит в byte quota. Synthetic CPU size-sweep: 6645 → 4751 новых blocks, 1095345 → 65536 size queries; это не Switch FPS. R10 transition fix сохранён. [Изменения/проверки](reports/matrixgame-e610-reuse-r11.md), [инструкция](docs/matrixgame-e610-reuse-r11-readme.ru.md). HARDWARE PENDING.

Предыдущий пакет r11: `dist/matrixgame-e610-switch-test-r11-reuse.zip`; source `203f563d6453b9a311af432a4e14428ad2f86ce7`, build `203f563d6453-f0031abf2bd4f34eacea`. В hbmenu **MatrixGame r11 Quick Smoke** / **MatrixGame r11 Interactive**. Содержит r10 transition fix. [SHA256 и evidence](reports/matrixgame-e610-reuse-r11.md). SOFTWARE PASS / HARDWARE PENDING.

### MatrixGame E610 r12: render state lookup без повторного поиска

Immutable metadata и правила render state вычисляются при компиляции; все 103 чтения, порядок проверок и ошибки сохранены. 4039 случаев совпали с frozen r11. Host ValidateDraw+Prepare: 103.7 → 23.3 мс / 20000 checks, около 4.45x конкретного CPU участка, не FPS. [Профиль/изменение](reports/matrixgame-e610-state-r12.md), [инструкция](docs/matrixgame-e610-state-r12-readme.ru.md). HARDWARE PENDING.

Актуальный пакет: `dist/matrixgame-e610-switch-test-r12-state.zip`; source `2bba518c902cea71df7320277f4380a88ffa7996`, build `2bba518c902c-8a2b24fe3b49770a447b`. В hbmenu **MatrixGame r12 Quick Smoke** / **MatrixGame r12 Interactive**. Содержит r10/r11 fixes. [SHA256 и evidence](reports/matrixgame-e610-state-r12.md). SOFTWARE PASS / HARDWARE PENDING.
