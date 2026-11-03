# MatrixGame E610 r11 — существующий DATA и два ручных запуска

**HARDWARE PENDING.** Этот вариант читает уже скопированные 2+ ГБ из
`sdmc:/switch/space-rangers-hd-a-war-apart/game/DATA` через RangersHostSession.
Повторная копия всего DATA в `matrixgame-diag/rangers` не требуется.
Основную NRO, ресурсы, настройки и сохранения не заменять.

## Копирование

Две NRO из этого архива скопировать в отдельную папку:

```text
<microSD>/switch/matrixgame-e610-r11-test/matrixgame-e610-quick-smoke.nro
<microSD>/switch/matrixgame-e610-r11-test/matrixgame-e610-interactive.nro
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

## Исправления r11 и сбор результатов

R11 включает исправление native transition из r10 и новые оптимизации
повторного использования draw upload memory. Аппаратные результаты r10/r11
пока не получены; тестировать достаточно этот новый пакет.

Пул хранит размер каждого завершённого блока в CPU metadata. Поиск больше
не вызывает SDK getSize для каждого кандидата. Upload может взять минимальный
подходящий свободный блок от requested size до 2x requested, в пределах
byte quota. Uniform blocks остаются строго 4096 байт. Ёмкость всего блока,
включая неиспользованный запас, учитывается в batch quota; после submit/wait
план пересчитывается по вернувшимся блокам. Лимиты: 64 live packets,
256 МиБ live packet bytes, до 128 / 16 МиБ дополнительных free blocks.
Адреса CPU/GPU upload block кешируются только в пределах unique owner;
при move/retirement обращения к старому owner отвергаются. Uniform GPU address
получается один раз на Bind, offsets и данные не меняются.

Новые counters: block_pool_larger_hits, upload_requested_bytes и
upload_reserved_bytes. Разность последних двух показывает cumulative padding.
Также остаются pool hits/misses, max_live_draw_bytes и profile.* timers.
Более высокий reuse и CPU-model counters не являются доказанным Switch FPS.
Порядок/число draw calls, содержимое, SDK flags, barriers, completion owners
и отказы ресурсов сохранены; GPU никогда не получает имитацию данных.

Transition из r10: при отсутствии native layout используется существующая
геометрия 1024x768/800x600 с масштабированием координат и скорости. Выполняются
настоящие loading capture, texture upload, Render и original Takt. Флаг не
снимается принудительно. В run.jsonl есть B.transition.geometry, source/target
extent и geometry_count. Требование четырёх настоящих map frames сохранено.

O2 с assertions, exceptions и настоящий debug deko3dd сохранены. Ресурсы,
сохранения, основная NRO и прежние пакеты не изменялись. Standalone entry
не вызывает Campaign/Gov, естественная победа и возврат Galaxy не проверяются.

Названия в hbmenu: **MatrixGame r11 Quick Smoke** и **MatrixGame r11 Interactive**.
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
точные исходники и сборку. Старые E610/r2/r3/r4/r5/r6/r7/r8/r9/r10 ZIP и основная NRO не изменены.
