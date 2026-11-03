# MatrixGame E610 r9 — существующий DATA и два ручных запуска

**HARDWARE PENDING.** Этот вариант читает уже скопированные 2+ ГБ из
`sdmc:/switch/space-rangers-hd-a-war-apart/game/DATA` через RangersHostSession.
Повторная копия всего DATA в `matrixgame-diag/rangers` не требуется.
Основную NRO, ресурсы, настройки и сохранения не заменять.

## Копирование

Две NRO из этого архива скопировать в отдельную папку:

```text
<microSD>/switch/matrixgame-e610-r9-test/matrixgame-e610-quick-smoke.nro
<microSD>/switch/matrixgame-e610-r9-test/matrixgame-e610-interactive.nro
```

Используются существующие файлы основной игры (только чтение):

```text
<microSD>/switch/space-rangers-hd-a-war-apart/game/INSTALL.TXT
<microSD>/switch/space-rangers-hd-a-war-apart/game/INSTALL_RUSSIAN.TXT
<microSD>/switch/space-rangers-hd-a-war-apart/game/CFG/...
<microSD>/switch/space-rangers-hd-a-war-apart/game/DATA/...
```

CFG и install-файлы должны относиться к той же копии игры, что и DATA.
Если ранее скопирован только DATA, остальные файлы брать из локальной
`windows/Space Rangers HD A War Apart`: INSTALL.TXT, INSTALL_RUSSIAN.TXT
и каталог CFG. Корневой пользовательский CFG.TXT, сохранения, EXE/DLL не нужны.
Не перезаписывайте неизвестные существующие файлы.

Отдельному оригинальному MatrixGame по-прежнему нужны ровно три файла:

| Источник в локальном проекте | Назначение на microSD |
| --- | --- |
| windows/Space Rangers HD A War Apart/DATA/robots.pkg | /switch/matrixgame-diag/game/DATA/robots.pkg |
| upstream/matrixgame/MatrixGame/CFG/robots/data.txt | /switch/matrixgame-diag/game/CFG/robots/data.txt |
| upstream/matrixgame/MatrixGame/CFG/robots/iface.txt | /switch/matrixgame-diag/game/CFG/robots/iface.txt |

Это около 233 МБ, а не повторная копия всего DATA. Если эти три файла уже
подготовлены для прежней диагностики, ничего копировать повторно не требуется.
Текстовые CFG берутся из закреплённого MatrixGame; robots.dat не подменяет их.
Раздельный MatrixGame root сохранён, чтобы не менять исходные игровые ресурсы.
SHA этих трёх файлов находится в manifest.json. Игровые ресурсы в ZIP не включены.
Шрифты/шейдеры встроены в NRO, внешний RomFS не нужен.

## Запуск и кнопки

Использовать имеющийся hbmenu в Application Mode с полной памятью приложения
(при стандартном override — удержание R при запуске установленного приложения).
Album/Applet Mode отклоняется. Ничего менять в системном ПО не требуется.
Фактический лимит и использование памяти фиксируются в run.jsonl.

1. Quick Smoke: без управления наблюдать до автоматического завершения или
   первого сбоя. Стадии: native/deko3d BC1/gamma smoke → оригинальный Init/карта
   → четыре настоящих видимых кадра карты → результат в память и cleanup.
   Загрузочные кадры не считаются. B/Plus отменяют тест и не означают PASS.
2. Interactive: после Init проверить изображение, курсор/кнопки, меню/паузу,
   камеру и доступные робот/звук/стрельбу. До 120 секунд после Init;
   штатный выход — Minus+Plus. Сначала отпустить кнопки после запуска.
   У Init отдельный кооперативный предел; блокирующие вызовы не пропускаются.

| Кнопка | Действие |
| --- | --- |
| Правый стик | Указатель |
| A / B | Левый / правый клик |
| D-pad | Прокрутка камеры |
| L / R | Поворот камеры |
| X | Вход/выход робота |
| Левый стик | Движение управляемого робота |
| ZR / ZL | Огонь / Shift |
| Y | Отмена приказа |
| Plus | Оригинальное меню |
| Minus+Plus | Завершение интерактивной диагностики |

Недоступные действия отметить как недоступные. При раннем сбое не пропускать
стадию принудительно. Это standalone Init/Draw; Campaign/Gov entry,
возврат Galaxy/Gov и естественная победа этими NRO не проверяются.
Новое аппаратное исполнение не означает заранее объявленный PASS.

## Исправления r9 и сбор результатов

В аппаратном r8 средний original Draw улучшился с 2.53 до 1.61 с, но
переход не завершился за 120 секунд. Профиль выявил большие CPU затраты;
compile commands r8 не содержали оптимизации (-O0 по умолчанию).

R9 собирает native translation units с -O2 -g -UNDEBUG -ffp-contract=off.
Assertions, exceptions и проверки ресурсов остаются активны. Настоящая
библиотека debug deko3dd сохранена; SDK и prebuilt Rangers archive не заменены.
Gate проверяет реальные compile_commands.json и linker maps, а compiler profile
включён в fingerprint, build-info.json и manifest.json.

Тот же production CPU decoder/validator сравнен на host при O0/O2:
checksum совпал, rejection guards сохранились. Скорость Switch нельзя
вычислить из host ускорения; r9 требует нового ручного запуска.

Пул завершённых блоков r8, exact uniform bytes, лимит 64 live packets,
managed residency, Init handoff, VI owner и orientation checker сохранены.
Журнал содержит block_pool_hits/misses и profile.* counters для диагностики.
Значения кумулятивны: сравнивать дельты totals, не вычитать max_us и не
складывать вложенные таймеры. Residency timer относится к Record image lookup,
а не ко всему texture decode/upload в других entry.
Таймаут, шаг игрового времени, transition и требование четырёх настоящих map
frames сохранены; загрузочные кадры не подменяют их. Campaign/Gov не вызывается.

Названия в hbmenu: **MatrixGame r9 Quick Smoke** и **MatrixGame r9 Interactive**.
Сначала запускайте Quick, затем Interactive. При FAIL сохраните журнал и фото,
не меняя ресурсы между запусками.

Журналы остаются отдельно от ресурсов:
`<microSD>/switch/matrixgame-diag/logs/<run-id>/run.jsonl`.
Там же при достижении соответствующих стадий появляются smoke.bgra,
gamma-smoke.bgra, orientation-smoke.bgra, original-first.bgra; размеры/формат BGRA8 указаны в журнале.
Прочий вывод — в output и rangers-user внутри папки запуска.
Для аппаратного дефекта нужны два новых каталога запуска и фото экрана,
при наличии — существующие crash reports из atmosphere/crash_reports или
atmosphere/fatal_errors. Системные настройки менять не требуется.
Не передавать игровые ресурсы и сохранения.

| Пункт | Quick | Interactive |
| --- | --- | --- |
| Init / карта / реальные кадры | NOT RUN | NOT RUN |
| Ориентация текста / картинки | NOT RUN | NOT RUN |
| Артефакты / зависания / скорость | NOT RUN | NOT RUN |
| Курсор / кнопки / меню / пауза / камера | NOT RUN | NOT RUN |
| Робот / звук / стрельба | NOT RUN | NOT RUN |
| Выход / crash reports | NOT RUN | NOT RUN |

SHA256SUMS.txt, manifest.json и build-info.json позволяют проверить файлы,
точные исходники и сборку. Старые E610/r2/r3/r4/r5/r6/r7/r8 ZIP и основная NRO не изменены.
