# E2E-5.70 — настоящий Rangers AFT text и nullable clip

Статус: **real-resource CPU text differential PASS; ASan/UBSan новых TUs PASS;
SDK archive compile / existing-bridge link PASS**. GUI, native Init и Switch
не запускались. Полный SDK host link и подключение сервисов к NRO остаются открытыми.

## Дефект и исправление

`TextRequest::has_clip=false` разрешён native bridge. Реальный исходный
`RobotRenderText` требует указатель на rectangle и без проверки выполняет
`SourceClip = pas::load_unaligned<Types::TRect>(Clip)`. В E569 producer направлял
RenderText прямо в эту функцию, поэтому отсутствие clip приводило к SIGSEGV.

Baseline использует сохранённый E569 producer, настоящий CacheData.dat,
настоящий `Font.2Normal` и исходный AFT rasterizer. Контрольный вызов с явным clip
успешен, следующий вызов без clip завершается signal 11. GDB подтверждает точную
строку разыменования Clip в оригинальном RobotRenderText; backtrace сохранён в
`build/matrixgame/e2e570-rangers-text/baseline/null-clip-backtrace.log`.

В `MakeRangersHostCallbacks` добавлен узкий `RenderHostText`:

- явный clip передаётся исходному renderer без изменения;
- при отсутствии clip создаётся rectangle `(0,0,width,height)`;
- нулевые width/height обрабатывает сам оригинальный renderer: он измеряет текст
  и расширяет right/bottom;
- текст, font, color, align, Wrap, offsets и image output передаются в настоящую
  RobotRenderText. Glyph/layout/rasterization не заменены;
- остальные 15 callbacks по-прежнему указывают прямо на оригинальные Robot functions.

Поведение generic `RangersBridge` для чужих callback providers не изменено:
отсутствующий clip по-прежнему передаётся им как nullptr. Преобразование находится
только в producer для конкретного исходного Rangers renderer.

## Реально исполненная цепочка

```text
original aPacket loose/package bootstrap
  → actual base Packages list from INSTALL.TXT
  → original CacheData.dat / Font.2Normal lookup
  → real data/font/Verdana_09_2.aft
  → original TCFontEC / RobotRenderText / TGraphBufGR
  → RangersBridge image ownership / RobotFreeText
```

Используется E569 shared sound/cache owner с SDL dummy для его звукового backend;
в этом probe не вызывается воспроизведение звука. Текст рисуется в настоящий CPU
ARGB buffer, без GUI, D3D/deko3d draw и без MatrixGame CGame::Init.

Первые два запуска остановились до null-clip проверки: для AFT не хватало
пакетов в сокращённой collection. Исправлен test bootstrap — original parser
читает **реальный INSTALL.TXT**, а package collection открывает его base list.
Проверки loader не отключались; файлы шрифта не подменялись. Первоначальные
failure result/log сохранены рядом с успешным baseline.

## Проверки и пределы

| Проверка | Результат |
| --- | --- |
| Baseline | Explicit-clip AFT control PASS, настоящий null-clip SIGSEGV воспроизведён и локализован GDB |
| Host | 24 пары candidate no-clip / original explicit-clip изображений совпали побайтно; варьируются Wrap и align flags |
| Crop | За пределами clip pixels = 0; внутри совпадают с original full image |
| Automatic dimensions | Original и candidate совпали, размер 156×16 |
| Error/cleanup | Некорректный Width=0 с Wrap=1 отвергается исходной функцией, native image не публикуется; Reset/FreeText и cache owner cleanup PASS |
| ASan/UBSan | Новые probe/bridge/producer/owner TUs PASS с detect_leaks=1/halt_on_error=1; 331 reused original TU не инструментированы |
| SDK | Новый adapter compiled в archive, старый bridge probe linked, strong undefined = 0 |

Сериализованные 24 буфера размером 200×64 совпадают также между normal и
sanitizer runs. Это сравнение с **прямым исходным AFT renderer на том же host**,
не с независимым Windows/D3D reference. Использован один реальный font и одна
строка с ASCII/кириллицей. Флаги Wrap проверены на этой строке; каталог fonts,
tagged/multiline layouts и весь UI этим не покрыты.

SDK probe не достигает новых host symbols; его ELF совпадает с E569, что
соответствует прежней bridge-only link проверке. Успешная линковка этого probe
не выдаётся за закрытый dependency graph нового host или audio/text runtime на Switch.
ARM64/QEMU full Rangers text runtime и аппаратная картинка не проверены.

[Exact evidence: команды, input/object/pixel hashes и backtrace](matrixgame-e2e570-rangers-text-evidence.json).

## Воспроизведение и артефакты

Нужны проверенные original-game host objects/headers из E569 и devkitPro SDK:

```powershell
python tools/matrixgame_rangers_text_checks.py baseline
python tools/matrixgame_rangers_text_checks.py host
python tools/matrixgame_rangers_text_checks.py asan
python tools/matrixgame_rangers_text_sdk.py
python tools/matrixgame_rangers_text_audit.py --output build/matrixgame/e2e570-rangers-text/audit-repeat.json
```

Для нового аудита выбирается новое имя. Baseline использует immutable
`producer-baseline.cpp` с SHA из E569 SDK metadata. Переиспользуются 331 проверенный
original object, включая два ранее обновлённых platform objects; большой game
build/GUI suite не повторяется. Проверки assertions активны через `-UNDEBUG`.

SDK archive: `build/matrixgame/e2e570-rangers-text/sdk/libmatrixgame-engine-callbacks.a`

SHA256: `6ce16e4ab56c1dbf57f2583c1b9cf36f6f028a04b094a8d646fd060bfcc4a35e`.

SDK existing bridge ELF:
`build/matrixgame/e2e570-rangers-text/sdk/matrixgame-engine-api-link-probe.elf`

SHA256: `e57c43663f032381622d3726cc05dc19b1af20dc5d1156f2c43e7e5c7181986e`.

## Точное состояние Init и следующая работа

Реальные sound/cache (E569) и text/free-text пути теперь проверены на host;
nullable clip в producer исправлен. Частичная таблица сервисов **не установлена
в standalone diagnostic**. Открыты music и валидация progress/render-release
context, полный Rangers SDK host link, подключение к native session и последующее
выполнение native Init/PrepareMap.

Последний аппаратный Init остаётся ReadParams → ApplyGammaRamp до map constructor.
Прежние gamma/settings/StoredStateBackend/minimap/F9 интеграции сохраняются.
Последняя отдельная NRO — E568; в этом этапе новая NRO не создавалась, поскольку
archive compile не доказывает готовность всего host executable. E568 NRO сохранена:
`d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750`.

Оба upstream clean, MatrixGame pin
`218115a72fa9c5d43269f4ca84a8596a0bcb82f1`. Protected inputs, INSTALL.TXT,
CacheData.dat/common/forms/Sound packages, E569 artifacts, frozen/resources/saves
и основные/прежние NRO сохранены. Commit/push нет; с устройством не взаимодействовали.
