# E2E-5.15: ошибка сохранения после настоящего перелёта

20.10.2026. Основа `2119202f41ce849c5f50ef888bf444df2f73716a`.
Исправление и повторяемый ресурсный маршрут проверены на ПК. На Switch/SD не проверено.
Host ELF SHA-256: `63e8d10c4036687afe697620ee69f76dca76a7e89b33699e22660e6ab0e5bcc2`.

## Найденная причина

После настоящего перелёта 41 → 5 инвентаризация ручного сохранения отклонила
`Script.PC_fem_rangers`, init-переменную `quest_item`: `UNKNOWN|unresolved`.
Это воспроизведённый host-дефект, а не предположение из аппаратного архива.
Run до исправления: `78bf06de0c1f4a78be356523a595d38c`, ELF
`a041dc0f1f4b576ff5f2aa67eb79f408fe65bcc865fc477fc61d47a829774c24`.
Перелёт завершился на 360-м ходу; ручной сейв не прошёл исходную fail-closed проверку.

Исходный ресурсный скрипт прочитан из scripts.pkg в приватную build-копию:
quest_item получает настоящий TItem через ShipItems/CreateEquipment. Скрипт не менялся.
Порт поддерживал typed refs для звезды/планеты/корабля/созвездия и восстановление
именованных script bindings, но не для такого живого предмета. Обнуление переменной,
усечение адреса до 32 бит и игнорирование ошибки не применяются.

## Правка игрового кода

| Файл | Функция | Изменение |
|---|---|---|
| e2e515_item_references.hpp | Find | Сравнивает непрозрачное значение только с адресами живых предметов в известных контейнерах. Не разыменовывает неизвестный адрес. Проверяет ненулевой ID и обратную идентичность через оригинальный IdToItem. |
| e2e5_save_inventory.hpp | classify / object_kind / RebindPending | Распознаёт найденный предмет, пишет его typed ID и восстанавливает предмет через IdToItem. Отсутствующий ID отвергается до изменения любых pending cells. |
| e2e5_save_refs.hpp | kItem / IsObjectKind | Добавлен reserved kind 15 для item ID в экспериментальном RSG64/v168. |

Обход учитывает инвентари, артефакты, guaranteed drops, магазины станций/планет,
планетные surface loot/warriors, player storage, свободные и stored items;
вложенные корабли Tranclucator обходятся с защитой от повторного посещения.
Необязательные контейнеры проверяются на nullptr. Финальное разрешение ID остаётся
за исходным TGalaxy::IdToItem. Не найденные в живых контейнерах и неразрешимые
ссылки остаются отказом сохранения, а не молчаливой заменой значения.

Новый reader загружает прежние RSG64/v168, что проверено исходным сейвом 346.
Сейвы с новым item-kind 15 требуют этой версии reader; прежние экспериментальные
reader с максимумом kind 14 их отвергнут. Формат Windows RSG/v167 не менялся;
обратная совместимость с Windows/старыми портами не заявляется. Исходные сейвы
и архивы не переписывались.

## Ресурсный маршрут и результат

Run `d98e9ae6ae8f42a0a2228105e3c95a13`:
копия сейва 346 → планета → ангар → штатный взлёт → карта → галактика →
касание видимой звезды → штатный ButJump → настоящие ходы/гиперпространство →
игровое меню → SaveManager → Unicode OSK → ButSave.
Все изменения мира выполняет исходная игра; драйвер передаёт только raw pad/touch.

| Проверка | Результат |
|---|---|
| Начальная система / цель / расстояние | 41 → 5, расстояние 7, исходная дальность 20 |
| Возвраты в orders mode | 347 / 347 / 357 / 360; часть ходов выполняет исходное автопродолжение прыжка |
| Расход топлива | 20 → 13 |
| Исходный экран гиперпространства | dispatch 19 подтверждён |
| Quest item | STABLE_OBJECT_ID / item / 182356 |
| Инвентаризация переменных | 873 проверено, 0 unknown |
| Физический сейв | «Перелёт Ёж 515.sav», 679360 bytes |
| SHA-256 сейва | `bc6b1059a743800a4ae279356fd8d58968fd6948deaafc34dac02582b8a1f6b0` |
| Прежние слоты приватной fixture | SHA сохранены |

Новый процесс, run `893f28c02fa34121b9dcaa57f10c9cd5`, загрузил именно этот SHA,
восстановил screen 16, ход 360 и систему 5. Все записанные checkpoint поля совпали:
`diff={}`, включая RNG, scripts/globals/inventory/ship hashes. scripts hash повторно
формируется исходным SaveState с typed IDs, поэтому это также проверка восстановления
предметной ссылки, а не только наличия файла. Полная сериализация всех объектов мира
побайтно не сравнивалась.

Пять кадров двух процессов проверены по RGB565 fingerprint и реальному владельцу
canvas; четыре кадра перелёта имеют последовательность 16/18/18/16. Контактный лист
с исходной системой, выбором цели, прибытием и reload просмотрен: планетный вид сменился
видом новой системы; map overlay сохраняет намеренную подложку. Это не полный C1 audit.

## Проверки и тестовая инфраструктура

Host mailbox получил bounded координаты касания: `sequence mask [x y]`, touch bit 1024,
панель 1280×720. Только host. e2e515_host_galaxy_snapshot.hpp атомарно пишет read-only
проекцию видимых звёзд исходным GalaxyPointToMapPoint. Наблюдение flight-orders пишет
только номер хода/систему/топливо/приказ, без сериализации во время смены режима.
Сериализованный checkpoint остаётся на штатных save/load границах.

Пять обычных engine tests PASS: runtime-loop, scene-canvas, screen-context,
native-focus, item-references. Отдельно sanitized-scene PASS (четыре renderer/GI units)
и sanitized-item-references PASS (actual aGalaxy/save refs + item test, ASan/UBSan,
leak detection). Остальные game units и ресурсный перелёт целиком не инструментированы.
Item test проверяет пустой граф, неизвестный адрес без разыменования, little-endian ID,
неизменность live cell, roundtrip, missing-ID без частичного восстановления и legacy guard.

Три preflight и devkitA64 compile десяти units PASS. Effective imports прежние:
254, dynamic calls 45, UNKNOWN 0; manifests меняются только по file count/callsites.
Нового NRO/full SDK link/CI нет. Восемь оригинальных пользовательских файлов неизменны.
Процессы остановлены runner после проверок; нормальный teardown не доказан.

## Неудачные кандидаты сохранены как FAIL

- `833f3f7d8f5942948bf995ec206a7ff7`: новая диагностика вызывала полную сериализацию
  при StartOrderMode и прервала переход при неподдержанном значении. В дальнейшем
  та же нехватка item support проявилась на настоящем ручном сохранении.
- `78bf06de0c1f4a78be356523a595d38c`: настоящий перелёт PASS, manual save FAIL из-за quest_item.
- `ef5e17a856974b769326639c35a44d38`: первый обход предметов завершился SIGSEGV;
  исправлены найденные незащищённые optional-null контейнеры. Кандидат не коммитился.
- Item test сначала не компилировался из-за абстрактных TShip/TItem и неверного List
  типа, затем выявил неполную инициализацию fixture. Исправлены конкретные TPlayer/TGoods
  и исходные обязательные контейнеры. Итоговый тест и санитайзеры PASS.
- Analyzer сначала требовал точное число flight records; исходный возврат из меню
  повторно публикует ту же orders state. Разрешены только идентичные post-arrival snapshots.

## Воспроизведение и открытые границы

```sh
python3 tools/e2e515_controller_route.py --load-save build/e2e514/recovered/Recovered-turn-346.sav --interstellar --label interstellar-item-final --game-root "$GAME" --config-root "$CONFIG"
python3 tools/e2e515_controller_route.py --load-save '<manual_save_path из result.json>' --expect-checkpoint build/e2e515/interstellar-item-final/result.json --label interstellar-item-reload --game-root "$GAME" --config-root "$CONFIG"
python3 tools/e2e515_engine_tests.py --sanitize-item-references
python3 tools/e2e515_engine_tests.py --sanitize-scene
python3 tools/e2e515_scene_frames.py build/e2e515/interstellar-item-final/runs/d98e9ae6ae8f42a0a2228105e3c95a13/result.json
```

Host commands — WSL; GAME/CONFIG read-only. Analyzer требует Pillow/NumPy и фактически
выполнен bundled Windows Python. SDK: `py -3 tools/e2e515_sdk_input_probe.py` в Windows.
Санитизированные метаданные: e2e515-interstellar-save.json; игровые ресурсы и сейвы в Git не входят.

SD/hardware save, другие виды ссылок и недостижимые IdToItem контейнеры, весь UI audit,
полный teardown, общие тормоза аркады, настоящий MatrixGame и финальный NRO/ELF остаются OPEN.
