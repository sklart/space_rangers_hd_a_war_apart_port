# MatrixGame E2E-5.27: первый диагностический запуск

Это отдельная диагностическая программа. SWITCH HARDWARE: NOT RUN /
HARDWARE_PENDING. Основная NRO Space Rangers HD не заменяется.
Подготовлен один бинарник; второй запуск планируется только после анализа первого.

## Что скопировать на SD

1. Из этой папки скопировать `matrixgame-e2e527-diag.nro` в
   `sd:/switch/matrixgame-diag/matrixgame-e2e527-diag.nro`.
2. Из своей личной тестовой копии ресурсов скопировать ровно три файла:

| Исходный файл в репозитории | Цель на SD |
| --- | --- |
| `windows/Space Rangers HD A War Apart/DATA/robots.pkg` | `sd:/switch/matrixgame-diag/game/DATA/robots.pkg` |
| `upstream/matrixgame/MatrixGame/CFG/robots/data.txt` | `sd:/switch/matrixgame-diag/game/CFG/robots/data.txt` |
| `upstream/matrixgame/MatrixGame/CFG/robots/iface.txt` | `sd:/switch/matrixgame-diag/game/CFG/robots/iface.txt` |

Точные абсолютные исходные пути, размеры и SHA-256 указаны в `resources.json`.
Игровые ресурсы в пакет не включены. Не копировать `robots.dat`: диагностическая
копия использует исходные текстовые CFG, проверенные Windows Init.
Шрифты и GPU shaders уже встроены в бинарник; отдельный RomFS не требуется.
`manifest.json`, `build-inputs.json`, `SHA256SUMS.txt` и `control.nacp` оставьте
на компьютере для проверки происхождения; hbmenu нужен только NRO.

Все записи программы направлены в отдельные уникальные каталоги диагностики.
Для основного игрового каталога, сохранений и старых frozen-комплектов действий нет.
Если папка `matrixgame-diag/game` уже существует, перед копированием сравните
её файлы с `resources.json`: не заменяйте неизвестные файлы автоматически.

## Как запустить один раз

Откройте hbmenu в application mode через запуск установленной игры с удержанием
R, если в вашей Atmosphère используется стандартный application override.
При изменённой конфигурации используйте назначенную там кнопку.
Источник: [официальный шаблон Atmosphère](https://github.com/Atmosphere-NX/Atmosphere/blob/master/config_templates/override_config.ini).
Запуск через Album обычно даёт applet mode; программа откажется до создания GPU.
Она дополнительно проверяет libnx applet type сама.

В hbmenu выберите **MatrixGame diagnostic**. Отпустите кнопки перед запуском.
Дождитесь автоматического возврата в hbmenu. B или + запрашивают выход на ближайшей
безопасной границе. Во время синхронного Init кнопка может сработать только после
его возврата. Управление боем этим запуском не проверяется.

Ожидаемая последовательность при успехе:

1. A: короткий smoke — фон, красный прямоугольник, оригинальный debug font.
   Он может смениться очень быстро и не является кадром MatrixGame.
2. B: экран original Init; во время загрузки он может оставаться неподвижным.
3. C: экран original Draw, затем настоящая карта, четыре завершённых кадра.
   Карта остаётся видимой ещё примерно две секунды.
4. D: cleanup и возврат в hbmenu. Финальное доказательство — журнал, а не экран.

При обычном исключении программа по возможности покажет FAILED, этап и краткую
ошибку на две секунды, выполнит cleanup и выйдет. При раннем отказе графики
экран может отсутствовать. При fatal deko SDK возможен штатный fatal-интерфейс
SDK без C++ unwind. Не считайте такой выход успешным cleanup.

Общий программный deadline 120 секунд проверяется на управляемых границах.
Он не способен прервать зависший блокирующий GPU/системный вызов. Если программа
зависла, зафиксируйте время и последний экран; завершите её штатным способом
через HOME, если это доступно. Не извлекайте SD, пока приложение выполняется.

## Что прислать после первого запуска

После выхода из приложения скопируйте **весь новый каталог**
`sd:/switch/matrixgame-diag/logs/run-…/` на компьютер и передайте его архивом.
Не ограничивайтесь последней строкой или скриншотом.

Внутри ожидаются:

- `run.jsonl` — появляется до первого GPU allocation; даже незавершённый файл полезен;
- `smoke.bgra` — если прошли smoke draw/readback;
- `original-first.bgra` — если оригинальная отрисовка достигла первого Present;
- `output/` — собственные результаты оригинального движка, включая кэш миникарты.

Если каталога нет, сообщите способ открытия hbmenu и видимое сообщение/экран.
Если есть несколько запусков, передайте все новые каталоги с указанием порядка.
BGRA — сырые пиксели по 4 байта; ширина/высота и SHA-256 находятся в журнале.
Отсутствие readback при раннем отказе ожидаемо. Последние `begin`, `last_ok`,
Result/exception и SDK context позволят установить реальную границу запуска.

Второй аппаратный запуск пока не выполнять: сначала анализируем этот комплект,
затем исправляем конкретную причину или назначаем проверку повторного Init/input.

## Воспроизведение сборки на текущем Windows workspace

Из корня репозитория, с установленными devkitA64/libnx/deko3d и уже подготовленными
оригинальными font bakes/исходниками E543:

```powershell
python tools/matrixgame_journal_checks.py host --output build/matrixgame/e2e544-smoke/journal
python tools/matrixgame_journal_checks.py arm64 --output build/matrixgame/e2e544-smoke/journal
python tools/matrixgame_smoke_checks.py host
python tools/matrixgame_smoke_checks.py arm64
python tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e544-smoke --diagnostic-source build/matrixgame/e2e543-journal/source
python tools/matrixgame_diagnostic_package.py
```

Используйте явный Windows Python из `manifest.json` / SDK commands, если `python`
указывает на MSYS2. Упаковщик откажется перезаписывать существующую NRO.
Точные configure/build/package команды и хеши входов сохранены в manifest.
Это воспроизведение из текущего подготовленного workspace, не обещание сборки
из одного чистого master без незафиксированных исходников и оригинальных assets.

HOST/ASan/UBSan и ARM64/QEMU подтверждают доступные CPU/файловые компоненты.
COMPILE/LINK подтверждает SDK ELF и отсутствие unresolved strong symbols.
WINDOWS HAL остаётся отдельным reference. Ни один из этих результатов не
подтверждает работу GPU, первый кадр или штатный выход на настоящем Switch.
