# MatrixGame E610 r40. Блоки draw и завершение Interactive

База исходников r39 — `c1116e01469e443cb5b9fd94ebfee8e3f8845b4d`, исходный
HEAD ветки — `ed26e8ac55870d3feb9b901ecc0e2c122f2ad782`. Git status перед
изменениями был чистым. Сверены 729 fingerprints базы и SHA256 ZIP r38/r39.
SHA256 `C:/logs6.zip` — `283063ce3c858dffd22d446464a347afd1822d3762bcfd602f8248d7d12492b6`.

## Аппаратная база r39

Quick. Init 30.816 с, 4 видимых кадра, код 0, полное время
43.826 с. Interactive. Init 30.694 с,
297 завершённых draw видимой карты, 2.605 FPS между первым и
последним draw. Код 10 при исключении кооперативного лимита. Последние строки
обоих JSONL подтверждают закрытие журнала без незавершённых операций.
SHA Quick — `e466b9f920d69e23ecd9f9712345b7a33b012edda77f39099c7f457c44efa990`.
SHA Interactive — `73da2228e0116dec772bf8d8831b8460dab2e238999f771904540a673eb05568`.
В ТЗ SHA Quick указан также для Interactive. Фактические значения в обоих
журналах совпадают с соответствующими записями манифеста r39.

Файлы `original-first.bgra` 1280×720 совпадают побайтно.
SHA256 — `7c76a5bc550e70aaaebe140c37aae5f0fe1db37d13da5cc1bf7163d00ccb8677`. Это базовый кадр r39. Снимок r40
будет сравниваться после аппаратного запуска с учётом времени анимации.

Interactive накопил 355727 записанных draw, 108145 созданий блоков и
13164666880 байт запрошенного upload. Между первой отметкой видимой карты
и итоговым snapshot было 334776 draw, 102950 созданий и 12404228096 байт
upload за 296 дополнительных кадров. Получается примерно 1131 draw,
348 созданий и 40 МиБ на видимый кадр. Число eviction 960 в обеих точках.

## Горячий путь и область правки

`C.original_form.Draw → SwitchBackend::Draw → CFormMatrixGame::Draw →
NativeDraw / NativeDrawUP / NativeDrawIndexed → Impl::Record →
NativeDrawUpload + PrepareUniforms → Bind / drawIndexed → SubmitAndWait
или PresentNativeFrame → waitIdle → ReleaseCompleted`.

`vertex_decode` заканчивается перед `Record` для трёх указанных native
entry. `draw_cpu` оборачивает `Record`, включая вызванный им `DrainBatch`.
`batch_wait` включает запись барьера, завершение команд, отправку, flush
и waitIdle. `command_encode` начинается после загрузки геометрии и
residency, но в r39 охватывает получение отдельного uniform-блока.
`block_create` включает создание блоков команд и draw, в том числе
вложенные в `command_encode` и `draw_cpu` вызовы. Разные места вызова и
вложенность не позволяют суммировать эти счётчики. Аппаратные накопленные
r39 значения в секундах — 87,54 draw_cpu, 33,12 block_create, 24,66
batch_wait, 13,73 command_encode, 11,84 vertex_decode.

В r40 отдельный блок uniform-данных заменён последней выровненной страницей
уже существующего блока upload. Его физическая ёмкость целиком участвует
в лимите батча. Uniform writer, входы вершин/индексов, дескрипторы, порядок
draw и shader offsets сохраняются. В `DrawPacket` один уникальный владелец,
`Binding` содержит адрес. Reset, abort, Present и drain используют прежнее
ожидание очереди. При неуспешном ожидании владельцы не возвращаются в пул.
Лимиты 64 draw, 256 МиБ in-flight и 16 МиБ пула сохраняются.

Профиль собирается в памяти, без записи на каждый draw. Snapshot включает
bucket запроса и тип draw / command spill / command fixed, requested и
reserved bytes, hits, miss по отсутствию подходящего размера, ограничению
reuse или лимиту пула, число и latency созданий. Повторные создания считаются
в пределах bucket после первого. Отдельно измеряются waitIdle при drain,
Present, acquire и abort.

## Проверки

| CPU-модель, 6762 draw | r39 | r40 |
| --- | ---: | ---: |
| Отправки | 108 | 108 |
| Создания / misses | 438 | 77 |
| Hits | 13086 | 6685 |
| Requested bytes | 290975744 | 290975744 |
| Reserved bytes | 308076544 | 302710784 |
| Пик активного батча, байт | 3055616 | 3006464 |
| Пик с пулом, байт | 3178496 | 3575808 |
| CPU total, медиана мкс | 239428 | 232002 |
| Draw CPU p50, медиана нс | 1769 | 1745 |
| Draw CPU p95, медиана нс | 4078 | 3594 |

Три запуска каждой версии в порядке ABBAAB. Последовательность размеров
синтетическая. Число draw на кадр взято из аппаратной базы. Модель использует
оригинальные заголовки layout/pool/batch из frozen r39 и те же входные данные
для r40. SDK handle endpoints реализованы на CPU. Время включает чтение и
хеширование для проверки порядка. Совпали полные байты vertex/index, всех
дескрипторов и uniform-страниц, хеш `14588887670708592657`. Сценарии отказа создания
и ожидания очереди проверяют сохранение владельцев до завершения. Пик с
пулом в этой последовательности выше на 388 КиБ.
Это оценка модели, GPU-время и FPS r40 определит аппаратный запуск.

Host ASan/UBSan, ARM64/QEMU UBSan и Windows O2 прошли fixture размещения
и владения, политику SessionControl и проверку durable JSONL. Проверено
срабатывание кнопок, фазовые лимиты и различие отменённой операции от
исключения. Ранний timeout Init остаётся ошибкой. Штатный лимит в игровом
цикле выходит через typed stop в общий teardown и даёт CONTROLLED_STOP,
`session_end_reason=time_limit` и код 0. Для Minus+Plus в игровом цикле
сохраняется код 20. Ошибка графики остаётся nonzero, экран ошибки требует
drawable и живого swapchain/queue. Реальные focus loss, reset и VI/deko3d
teardown ещё предстоит проверить на Switch.

Воспроизведение из корня репозитория: `python tools/matrixgame_r40_checks.py
benchmark`. Другие режимы — `host`, `arm64`, `windows`. Полные результаты
находятся в `build/matrixgame/e610-r40-checks`.

Отдельная host-проверка production lifecycle с IPC-моделью прошла сценарии
focus/resume/background, выход, восстановление видимости и ошибки. SDK
собрал и слинковал обе NRO. В обеих ELF присутствуют `PrepareUniformSlice`,
`RunSmoke`, оригинальный цикл и teardown. Проверка неразрешённых сильных
символов прошла. Сверены SHA256 730 исходников SDK
и 416 защищённых файлов. Сгенерированный оригинальный
код побайтно совпадает с r39. Каждый NRO увеличился на 4096 байт.

Статус — **SOFTWARE PASS / SWITCH HARDWARE PENDING**.
Build ID — `8865c0a67970-900c6d61cfe3ff1f04c1`.
Исходник — `8865c0a679703c0ac1bdb71bf67f0b06c15da8cb`.

| Файл | Размер, байт | SHA256 |
| --- | ---: | --- |
| ZIP | 10163390 | `70c02be56a785708ca1ea1aeb3a414f99a894792670f552ef559460599d08121` |
| Quick Smoke NRO | 13617349 | `daf76807eb0ab96855b59cfd4e294952f009bef80a29a4ee0399adde0d93068d` |
| Interactive NRO | 13617349 | `348b1962b3255b717a30ab1e406e7351d140727f784e4953b1714c886281e341` |

Архив расположен в `dist/matrixgame-e610-switch-test-r40-draw-buffers.zip`.
Штатный детерминированный GPU smoke проверяет BC1, gamma и ориентацию.
Фактическое исполнение r40, совпадение smoke-пикселей и кадра карты, GPU-время
и чистый выход по лимиту предстоит подтвердить на Switch. Для двух прогонов
сначала запустить Quick, затем дать Interactive достичь лимита.

[Протокол проверок](matrixgame-e610-draw-buffers-r40-evidence.json).
[Инструкция через NetLoader](../docs/matrixgame-e610-draw-buffers-r40-readme.ru.md).
