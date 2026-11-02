# MatrixGame E610 r2: аппаратные журналы от 02.11.2026

**HARDWARE PARTIAL: GPU SMOKE / PREPAREMAP PASS; FULL INIT FAIL.**
Приёмка MatrixGame на Switch не завершена. Проверены три журнала и снимок
экрана из пользовательского `C:/pack.zip`; новые сборки и runtime-тесты не запускались.
Архив не изменялся. Вложенные файлы рассматривались как данные.

SHA256 архива: `73dd88563bcbaa8f36432f7bf962a0e4c18a6fdc12a92fe225081ab342317e36`.
Размер: 67050 байт. Build ID всех запусков:
`8c3612d2c347-21120b4e60dc014c05ec`.
Self-NRO hashes совпали с manifest r2: два Quick, один Interactive.

| Run ID | Режим | GPU smoke / PrepareMap | Итог |
| --- | --- | --- | --- |
| run-1793618087072125-0 | Quick | PASS / PASS | Журнал обрывается на failed B.CGame::Init, 136 записей; дальнейший исход неизвестен |
| run-1793618118070492-0 | Quick | PASS / PASS | ApplyVideoParams FAIL; viOpenDefaultDisplay failed: 4722; exit 10, journal_close |
| run-1793618567070007-0 | Interactive | PASS / PASS | Та же ошибка; exit 10, journal_close |

## Что действительно выполнено на Switch

Во всех запусках прошли native device/queue/shader setup, настоящий BC1
upload/draw/readback и gamma presentation readback. Все шесть файлов BGRA имеют
размер 1280×720×4, их SHA256 совпадают с журналами. Содержимое соответствующих
smoke и gamma-smoke файлов одинаково между тремя запусками. Центр BC1:
BGRA [0,0,255,255]; gamma: [191,127,64,255], в пределах исходного допуска ±1.
Это аппаратное доказательство узкого smoke, не кадры игры и не полный GPU PASS.

RangersHostSession и выбранные пути ресурсов прошли свою начальную инициализацию.
Оригинальная цепочка дошла через OpenPackFiles → text config → CacheInit →
platform attachment → map constructor → RobotPreload → stor.Load →
BuildTexUnions → PrepareMap. Следовательно, прежнее предположение о простом
отсутствии ресурсов не объясняет эти сбои. Полнота всех поздних ресурсов не
проверена, поскольку Init не завершён.

Следующая операция — B.interface_and_final_resources / B.ApplyVideoParams —
падает во всех трёх журналах. Полный Init, четыре видимых кадра карты,
интерактивный loop, игровые HID/звук/стрельба/пауза и штатный Minus+Plus не
подтверждены. В двух полных журналах error-screen и освобождение host/device
дошли до journal_close; успешный игровой cleanup этим не доказан.

## Локализованный сбой

В двух полных журналах исключение: `viOpenDefaultDisplay failed: 4722`.
В текущем исходнике `DekoDevice::ReadAdapterDisplayMode` вызывает
`viOpenDefaultDisplay` при уже существующем default NWindow, затем намеревается
запросить resolution и VSync. Ошибка происходит до этих запросов.
DeviceError выводит код десятичным числом: 4722 = 0x1272, module 114 / description 9.
Смысл service error по одному числу здесь не объявляется установленным.

Область следующего исправления — владение/переиспользование VI display при
запросе режима; гипотезу повторного открытия нужно проверить по реализации
используемого libnx. Подмена запроса константами или принудительное продолжение
Init не допускаются. Код пока не изменён, исправление не заявлено.

## Ориентация

На снимке `2026110214225800.jpg` видны строки **Stage B: original MatrixGame Init**,
**Minus + Plus**, путь logs. Это экран стадии B, а не FAIL. Текст и положение
строк перевёрнуты по вертикали; горизонтальное отражение снимком не подтверждается.

Сохранённый smoke framebuffer, декодированный как верхний левый BGRA8 без
переворотов, содержит нормально ориентированный текст сверху слева. Значит,
в данном readback шрифт не отражён. Точный источник дефекта экранного вывода
пока не доказан: нужно отдельно сопоставить status rendering, presentation/gamma
и scanout. Gamma smoke использует постоянную LUT, поэтому его равномерная
картинка не доказывает правильную ориентацию изображения.

## Сохранённые результаты и дальнейшая граница

- Машиночитаемое [evidence](matrixgame-e610-r2-hardware-20261102-evidence.json)
  содержит SHA, стадии, ошибки и пиксели трёх запусков.
- Исходные журналы, BGRA и фото сохранены локально в
  `build/matrixgame/e610-hardware-pack-20261102/`; исходный ZIP сохранён на месте.
- Основная NRO, старые диагностики, ресурсы, сохранения и upstream не изменялись.
- Следующие адресные работы: реальный VI display owner/query и вертикальная
  ориентация presentation; затем повторный Quick до четырёх кадров.
- Campaign/Gov, победа, battle result return на Switch и полный физический
  HID/GPU остаются непроверенными. Большие Windows/host регрессии не повторялись.
