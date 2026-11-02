# MatrixGame E610 r4: аппаратный результат 02.11.2026

**HARDWARE PARTIAL: managed residency работает; Init останавливается на миникарте.**
Проверен пользовательский C:/logs1.zip, 139059 байт, SHA256
`edfaa750f41430d5dd8ccbd307fdd351b14f1f83dbcf8c73e9604fbbc9dc6aad`.
Два новых журнала содержат по 4699 последовательных записей и корректный journal_close.
Self-NRO hashes совпали с финальными Quick/Interactive r4;
build ID `d5af0be47d5d-65a4fae0c985699dfa07`, source
`d5af0be47d5d48b8d8f392c403272cb6f9486026`.

| Run | Режим | Managed evictions | Итог |
| --- | --- | ---: | --- |
| run-1793624935006824-0 | Quick | 474 | CMinimap::RenderBackground FAIL, exit 10 |
| run-1793625464003653-0 | Interactive | 474 | Тот же отказ, exit 10 |

## Подтверждённое на Switch

ApplyVideoParams и identity presentation прошли в обоих запусках. Все шесть BGRA
файлов имеют размер 1280×720×4, SHA совпали с журналами. Независимое сравнение
smoke и orientation-smoke даёт max RGB difference 0, alpha полностью совпадает.
Подтверждение фактической ориентации экранного scanout отдельно не приложено.

В обоих прогонах зафиксированы 474 arena pressure events и 474 managed evictions.
GPU upload продолжился после исчерпания диапазонов фиксированной арены 64 МБ;
прежнее исключение original managed texture decode failed: -1 не повторилось.
Это аппаратное подтверждение работы eviction/retry в достигнутой цепочке,
не доказательство всех сценариев residency, производительности или полного боя.

## Новая граница

Первый failed scope — B.CMinimap::RenderBackground. Далее распространение
исключения помечает interface_and_final_resources, CGame::Init и enclosing owners.
Финальная ошибка в обоих запусках:

`native vertex binding differs from retained staging context`

Её источник — DekoDevice::BindStagingVertex в platform_switch.cpp: guard сравнивает
stream/buffer/offset/stride с buffers::Current().Vertex(). Сам guard не записывает,
какое именно поле отличается; по этим журналам нельзя выбрать конкретную
первопричину или объявить виновным только stride. Нужно воспроизвести согласование
staging Context с native binding при подготовке/восстановлении состояния миникарты.
Пропуск guard или фиктивный успех BindStagingVertex не являются исправлением.

Полный Init не завершён, original loop не запущен, original-first.bgra отсутствует.
Счётчик completed_presents=4 относится к диагностическим/status кадрам;
это **не** четыре принятых игровых кадра карты. Отсутствуют сообщения
B.complete_original_Init и original_loop_frames_presented_final.

Error-screen и освобождение host/device/VI дошли до закрытия журналов.
События failed A.SwitchDisplay / A.native_device_owner отражают позднее
исключение; сами по себе они не означают новый отказ создания display.
Точные counters и VI cleanup results сохранены в machine evidence.

## Сохранённые результаты

[Машиночитаемое evidence](matrixgame-e610-r4-hardware-20261102-evidence.json)
содержит хеши NRO, журналов, кадров, counters и точку отказа. Неизменённые
журналы/кадры сохранены в build/matrixgame/e610-r4-hardware-20261102/;
исходный ZIP не изменялся. Исходники, NRO, игровые ресурсы, сохранения и upstream
при анализе не менялись, новые сборки или runtime-тесты не запускались.

Следующая адресная задача — vertex staging/binding и state restoration в
CMinimap::RenderBackground. Аппаратный PASS Init/карты, физический игровой HID,
Campaign/Gov, победа и полный battle loop остаются открытыми.
