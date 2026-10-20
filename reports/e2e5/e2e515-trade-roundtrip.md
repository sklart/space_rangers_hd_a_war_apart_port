# E2E-5.15: количество, покупка, продажа и сохранение

20.10.2026. Основа `2a4077dfa15fc4bec00a00c5c682e8a4a6942e6e`, последующие
изменения перечислены ниже. Host ELF SHA-256:
`3d472cc1b1999cd603ca74ce9f09940d83b948287b378ef40591e14b7487948c`.
Общий статус PARTIAL: конкретный торговый маршрут проверен на ПК, Switch отсутствует.

## Изменения

| Файл | Функция | Правка |
|---|---|---|
| controller_ui.hpp | Process | В настоящем TfCount2 NavigateLeft/Right вызывает оригинальный MainPanelKeyDown. Один шаг/повтор сохраняет прежние Minimum/Maximum/RefreshValue. Mouse repeat timer не создаётся. |
| e2e515_save_checkpoint.hpp | Observe | Только host opt-in: числовые деньги, свободный трюм, восемь cargo/stock/cost/buy/sell записей перед/после операции. Игровая модель не меняется. |
| e2e515_controller_route.py | --trade-roundtrip | Реальный pad-маршрут: Cancel, обе границы количества, покупка/продажа одной единицы и проверки числового баланса. Совместим с исходным Unicode save route. |
| e2e515_scene_frames.py | trade analysis | Проверяет actual stderr records, себестоимость этой fixture и владельцев/хэши семи кадров. |

Предыдущий пакет оставлял только курсорный путь изменения количества.
Теперь D-pad влево/вправо меняет значение, вверх/вниз перемещает focus между кнопками.
Оригинальные Ok/Cancel и cursor slider/Add/Sub сохранены. Последние курсорные
операции отдельно не проверялись. В начале окна Ok остаётся выбранным; удержание
открывающей A не подтверждает покупку.

## Результат на реальном сейве

Исходный восстановленный сейв 346-го хода скопирован в новый user root. Originals
read-only. Итоговый run `d8173c76af0442a5b556f84776166d25` находится в
`build/e2e515/goods-roundtrip-save/runs/`. Это реальные игровые объекты и handlers,
не установка количества/денег тестом. Первоначальный Cancel сохраняет записанный
checkpoint. Для покупки и продажи проверены достигнутый максимум и ещё один шаг
вправо, затем достигнутый ноль и ещё один шаг влево. После шага вправо выбран ровно 1.

| Поле | До покупки | После покупки | После продажи |
|---|---:|---:|---:|
| Деньги | 2044 | 2012 | 2043 |
| Продукты в трюме | 0 | 1 | 0 |
| Продукты рынка | 23 | 22 | 23 |
| Свободный трюм | 54 | 53 | 54 |
| Себестоимость продуктов | 0 | 32 | 0 |

Покупка за 32, продажа за 31 — исходные цены с учётом текущего навыка. Другие семь
товаров не изменились. Семь реальных PPM проверены по RGB565 fingerprint, screen 34,
глубине и registered_owner (родитель 1, количество 2, возврат 1). Контактный лист
trade-contact.png просмотрен; окна покупки/продажи показывают одну единицу, подложка
магазина корректна. Метаданные проверки: scene-analysis.json в каталоге run.

После торговли исходный SaveManager записал «Торговля Ёж 515.sav», 798430 bytes,
SHA-256 `9a93bba2b9ea185087f50cba14baf8a797564cd2b6a2a20c11e88310a68d1beb`.
Сохранение подтверждено worker commit/readback и физическим файлом; исходный слот
не изменился. Загрузка новым процессом PASS в run `3931579a7ad3470bab75f3788d3e61be`:
восстановлен screen 34, подтверждён его registered-owner кадр (depth 1), все поля
записанного checkpoint совпали (`diff={}`). Восемь кадров обоих процессов проверены
по fingerprint. Санитизированные метаданные: e2e515-trade-roundtrip.json.
Это сравнение записанных полей checkpoint, не полной сериализации всех объектов мира.

Первый reload run `906212b4801e4bde9a1d6d236a539a68` завершился FAIL драйвера:
его restored_screen принимал только 4/5/6/16, тогда как сохранение вернуло screen 34.
Actual dispatch, SceneCanvas 34, первый кадр, focus товарных строк и совпадающие load
checkpoint присутствовали. Список драйвера дополнен именно проверяемым экраном 34;
после checkpoint comparison теперь требуется подтверждённый present с реальным
владельцем восстановленного экрана. Старый FAIL не переименован.

Run `103fd978655d48d28e3badc4fc6a845f` завершился другим FAIL: 80-секундный budget
старта истёк до первого кадра меню, во время чтения GAI FormMain3.2ShipA3, после
двух анимаций по 58/62 MB. Загрузка сейва не начиналась. Budget именно старта
главного меню увеличен до 180 секунд; остальные ожидания не менялись. Это не
измерение игрового FPS и не основание объявлять предыдущий запуск успешным.

## Проверки и воспроизведение

Пять engine tests PASS, включая частичный sanitized-scene. Его ASan/UBSan scope:
renderer_platform, GR_Main, GR_GraphBuf, GI_MessageLoop и scene test. Полный торговый
маршрут не инструментирован. DevkitA64 Save ON compile девяти units PASS, без full
SDK link/NRO. Первый запуск SDK probe в WSL использовал Windows compiler path и
завершился FileNotFoundError до компиляции; повтор выполнен через Windows Python.
Восемь исходных файлов данных остались неизменны. Подмодули не менялись.

Команды host выполняются в WSL из корня checkout; GAME/CONFIG обозначают исходные
каталоги ресурсов и конфигурации только для чтения:

```sh
python3 tools/e2e515_engine_tests.py --sanitize-scene
python3 tools/e2e515_controller_route.py --load-save build/e2e514/recovered/Recovered-turn-346.sav --trade-roundtrip --resave-name 'Торговля Ёж 515' --label goods-roundtrip-save --game-root "$GAME" --config-root "$CONFIG"
python3 tools/e2e515_controller_route.py --load-save '<manual_save_path из result.json>' --expect-checkpoint build/e2e515/goods-roundtrip-save/result.json --label goods-roundtrip-reload --game-root "$GAME" --config-root "$CONFIG"
python3 tools/e2e515_scene_frames.py build/e2e515/goods-roundtrip-save/runs/d8173c76af0442a5b556f84776166d25/result.json
```

Последняя команда требует Pillow/NumPy; фактически выполнена bundled Windows Python.
SDK: `py -3 tools/e2e515_sdk_input_probe.py` в Windows. Ранний успешный маршрут без
проверки верхней границы/сохранения: `f7252085ffb246459ab4c94f63195640`; итоговые
утверждения выше относятся к полному повтору d8173c76.

## Открытые границы

Другие товары/локации, запрещённый товар и подтверждение предупреждения, нехватка денег,
полный трюм, массовая покупка, cursor drag/hover и весь экономический паритет OPEN.
Процессы остановлены runner после проверок, нормальный teardown не доказан.
Нового NRO, CI и аппаратного PASS нет. Общие тормоза аркады, межзвёздное сохранение,
оставшиеся UI-механики и настоящий MatrixGame продолжают большую цель.
