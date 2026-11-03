# MatrixGame E610 r8 — существующий DATA и два ручных запуска

**HARDWARE PENDING.** Этот вариант читает уже скопированные 2+ ГБ из
`sdmc:/switch/space-rangers-hd-a-war-apart/game/DATA` через RangersHostSession.
Повторная копия всего DATA в `matrixgame-diag/rangers` не требуется.
Основную NRO, ресурсы, настройки и сохранения не заменять.

## Копирование

Две NRO из этого архива скопировать в отдельную папку:

```text
<microSD>/switch/matrixgame-e610-r8-test/matrixgame-e610-quick-smoke.nro
<microSD>/switch/matrixgame-e610-r8-test/matrixgame-e610-interactive.nro
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

## Исправления r8 и сбор результатов

В r7 оба запуска прошли Init и достигли original Draw/Present, но Interactive
показал только 48 transition frames за 121.8 с (около 0.39 FPS). Ни один запуск
не завершил переход и не получил четыре зачтённых игровых кадра.

В r8 добавлен пул только завершённых draw upload/uniform blocks. Форматы,
GPU flags, offsets, draw calls и лимит 64 одновременно живых packets сохранены.
Пул хранит не более 128 блоков / 16 МиБ; same-size reuse не меняет batch budget.
До completion каждый packet владеет блоками отдельно. Ошибки completion не
возвращают их в пул; cleanup освобождает пул после ожидания/закрытия очереди.
Дополнительно к живым packets могут храниться свободные блоки пула; их число
не входит в max_live_draw_packets, оно ограничено отдельно.

Повторяющиеся успешные batch operations заменены агрегированными таймерами.
Texture pressure сохраняет первые 4 события и степени двойки, а полный счётчик
записывается отдельно. Ошибки, Init/Draw границы, результат и cleanup продолжают
записываться с fflush+fsync. Фактический отказ allocation/queue не скрывается.
Журнал содержит profile.* для draw CPU, vertex decode, новых SDK allocations,
residency, command encoding, batch wait и реального journal IO. Значения
кумулятивны; сравнивайте разности. Вложенные таймеры нельзя просто складывать.
Counters снимаются также на ранних и каждых 30 переходных кадрах.

Init handoff r7, managed residency, staging setters, VI owner, orientation
checker и настоящий debug SDK deko3dd с linker-map проверкой сохранены.
Host/ARM64 проверки CPU payload/lifetime и host file IO не являются замером
Switch FPS. Достижение требуемой скорости r8 предстоит проверить вручную.
Игровое время, transition, обязательные draw и контракт 120 секунд не изменены.

Названия в hbmenu: **MatrixGame r8 Quick Smoke** и **MatrixGame r8 Interactive**.
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
точные исходники и сборку. Старые E610/r2/r3/r4/r5/r6/r7 ZIP и основная NRO не изменены.
