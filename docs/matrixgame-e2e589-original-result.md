# MatrixGame E589: результат и штатная сдача

Исправления длинного боя и актуальные отдельные NRO: [E590](matrixgame-e2e590-completed-battle.md).
Ниже сохранены данные неизменных E589 artifacts.

Новые отдельные обычные SDK NRO, **на Switch ещё не запускались**:

| Режим | Путь | SHA256 |
| --- | --- | --- |
| Manual | `build/matrixgame/e2e589-original-result/sdk/interactive-nro/matrixgame-native-interactive.nro` | `b6bf87f02a6965ec6b93b6dc46276472724fadea2b97780b88baed10fd2c78ec` |
| Quick | `build/matrixgame/e2e589-original-result/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro` | `a5b7ef77fbc44d878db3a23486dad54707623dd24aa21e8138324fd791766cb7` |

Обе — 14899397 bytes, source-family build ID
`82034333146d-915afb38dd6560bc7962`. Основная и E586 NRO сохранены.
Ресурсные roots/private outputs, штатные controller bindings, 120-секундные
cooperative deadlines и ограничения будущего запуска прежние:
[инструкция E586](matrixgame-e2e586-interactive-diagnostic.md).

Manual теперь допускает штатный исход сдачи `original_exit_state=4`.
При обычном возврате original loop он снимает настоящую CGame::SaveResult
статистику до освобождения карты. В run.jsonl появляются операция
`C.original_SaveResult` и шесть записей с event=`statistics`:

- `original_time_signed_ms`;
- `original_robots_built`, `original_robots_destroyed`;
- `original_turrets_built`, `original_turrets_destroyed`;
- `original_buildings_destroyed`.

Значения находятся в signed поле **result**, а не unsigned value. Это сохраняет
знак исходного времени. Quick тоже снимает статистику после своих четырёх
видимых кадров; такой ранний snapshot не является результатом завершённого боя.

При будущей пользовательской проверке manual можно использовать штатное меню
игры для сдачи, подтвердить её и закрыть статистику. Требуются минимум четыре
видимых map frames, original_exit_state=4, успешная C.original_SaveResult и
последующий cleanup. Отмена через Minus+Plus по-прежнему даёт process_exit=20;
таймаут остаётся ошибкой. Ни отмена, ни таймаут не создают подставной результат.

Windows HAL проверил keyboard-путь меню/сдачи дважды. Физический Switch HID,
native/deko Init/map и hardware recovery не проверены. Полный бой с победой/
поражением и переход обратно в галактическую часть остаются открытыми.
[Доказательства и воспроизведение](../reports/matrixgame-e2e589-original-result.md).
