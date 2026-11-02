# E2E-5.89: original SaveResult и штатный выход через сдачу

**PASS: два Windows HAL hosted-прогона; CPU policy Windows/ASan/ARM64;
две отдельные SDK NRO собраны, Switch NOT RUN.**
[Машинные доказательства](matrixgame-e2e589-original-result-evidence.json).

## Исправления production

В `NativeInitRuntime` добавлен `SaveOriginalGameResult(SRobotGameState&) const`.
Он требует текущего живого full-game owner, завершённого original Init, карты
и player side. Вызывает настоящий `CGame::SaveResult` в локальный snapshot,
после успеха копирует все шесть полей вызывающему. При отказе до Init/после
Deinit выходной объект остаётся неизменным. Snapshot не объявляется исходом боя.

Native diagnostic вызывает этот API после обычного возврата original loop и
до `Deinit`. Все шесть исходных статистик пишутся в `run.jsonl`. Signed time
передаётся через signed `Record.result`, без преобразования в unsigned Metric:
исходная игра использует знак времени для победившей стороны. Cancel/timeout
по-прежнему остаются отменой/ошибкой, искусственный результат для них не создаётся.

Interactive policy раньше разрешала original exit state 0..3. Исходный
`OkSurrenderHandler` выставляет **4**; это законный исход, который диагностическая
обвязка ошибочно отклоняла. Теперь interactive принимает 0..4 после минимум
четырёх видимых кадров. Quick по-прежнему принимает только 0. Отрицательные,
5 и exception-коды 100..104 отклоняются.

## Воспроизведённый дефект и runtime

Baseline использовал копию E586 SessionControl с точным историческим SHA256
из предыдущего evidence. При четырёх подтверждённых кадрах `ValidLoopReturn(4)`
вернул false; отрицательный oracle завершился ожидаемым кодом 23. Копия,
probe, команды, binary и логи сохранены в `build/matrixgame/e2e589-original-result/baseline`.

Новый Windows сценарий использует настоящий owning Rangers host, original
full Init, map/transition, CFormMatrixGame/L3GRun и EngineSink keyboard:

1. Обычные startup/Enter/Pause/menu/camera проверки.
2. `Esc` открывает штатное меню, `S` открывает подтверждение сдачи.
3. `Enter` исполняет original handler: g_ExitState становится 4; следующий
   original Takt показывает статистику.
4. Второй `Enter` исполняет исходный выход из статистики и завершает loop.
5. После focus release/recreate и четырёх readbacks новый API снимает результат;
   каждое поле сверяется с настоящим player-side GetStatValue.
6. После `Deinit` ReadEngineStatistics сохраняет все значения; owning Rangers
   host ещё жив. Попытки SaveResult до Init и после Deinit отклоняются,
   шесть sentinel полей не изменяются. Затем освобождается сам host.

`g_ExitState=4` и финальный GFLAG_EXITLOOP выставляли исходные UI handlers.
Тест обнуляет exit state перед loop, как исходный CGame::RunGameLoop; исход боя,
HP, позиции и статистика не подставляются.

Обе сессии: **208 loop Draw / 5050 мс map simulation**, штатный код 4,
8 суммарных readbacks, восстановление gamma/window/device/timer leases и
двойной Init/Deinit/global cleanup PASS. Все пять build/kill counters — 0,
исходный STAT_TIME — **47599 и 45564** соответственно. STAT_TIME в оригинале
обновляется через `timeGetTime() - GetStartTime()`, поэтому он не равен
deterministic map-loop clock и не обязан совпадать между запусками.
Победа/поражение и отрицательное время победившего игрока здесь не воспроизводились.

## Проверки и сборка

| Gate | Область и результат |
| --- | --- |
| Windows HAL runtime | Настоящие original menu handlers, новый SaveResult wrapper, все шесть статистик, pre/post rejection, host/MatrixGame cleanup — PASS ×2. Новый original_game.cpp скомпилирован и включён напрямую поверх неизменных E583 archives. |
| Windows CPU policy | Оба режима/16 button combinations, сроки/frames/reuse, surrender4 и отказ 100..104 — PASS. |
| Host ASan/UBSan | Та же portable policy выполнена с detect_leaks/halt_on_error — PASS. |
| ARM64/QEMU | Та же portable policy скомпилирована и выполнена — PASS. |
| devkitA64/libnx/deko3d | Actual native sources, оба diagnostic targets, link/package/font checks, zero strong undefined — PASS. |
| Source/artifact audit | Current own sources, generated originals, link/binary/outputs, PNG oracle, baseline и fingerprints защищённых входов — PASS. |
| Switch runtime | NOT RUN. |

**Live CGame::SaveResult здесь исполнялся только Windows HAL.** Sanitizer/QEMU
runtime proof относится к portable session policy, не ко всему original Init/
SaveResult. SDK является compile/link/package proof. Большие прежние suites
не повторялись. Generated original algorithms совпали с E586; upstream не менялся.

Первая SDK попытка успешно прошла compile/link, но source audit отклонил её:
во время первоначального формирования metadata был исправлен файл аудитора
(требование одинакового STAT_TIME между прогонами). Эта попытка не объявлена
PASS. Семь её artifacts/metadata/result сохранены в `sdk-first-attempt` с SHA256.
После фиксации исходников повторный configure и incremental build обновили
metadata/entries и дали финальный PASS без повторного runtime сценария.

## Новые отдельные NRO

Source-family build ID: **`82034333146d-915afb38dd6560bc7962`**.

| Артефакт | Bytes | SHA256 |
| --- | --- | --- |
| `build/matrixgame/e2e589-original-result/sdk/interactive-nro/matrixgame-native-interactive.nro` | 14899397 | `b6bf87f02a6965ec6b93b6dc46276472724fadea2b97780b88baed10fd2c78ec` |
| `build/matrixgame/e2e589-original-result/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro` | 14899397 | `a5b7ef77fbc44d878db3a23486dad54707623dd24aa21e8138324fd791766cb7` |

Это прежние отдельные стандартные targets `matrixgame-native-interactive-nro`
и `matrixgame-native-diagnostic-nro` в новом build directory. Основная и обе
E586 NRO сохранены. Размеры двух новых NRO совпали из-за выравнивания;
SHA256, entry policies и NACP titles различны. Обе содержат настоящие
SaveOriginalGameResult/CGame::SaveResult symbols и telemetry strings.
[Инструкция будущего ручного теста E589](../docs/matrixgame-e2e589-original-result.md).

## Точная граница

Теперь native entry действительно снимает исходную статистику перед teardown;
Windows доказал legal surrender → statistics → loop return → snapshot → cleanup
с живым owning host. Это не полный бой до победы/поражения и не переход в
галактическую часть игры. Diagnostic host предоставляет services/data, но
такой galaxy transition в этом сценарии не запускался.

Остались full native/deko Init с видимой картой, аппаратные image/audio/HID/F9/
focus/recovery, полный бой с победой/поражением и owning galaxy transition.
Последняя реальная Switch boundary из logs2 прежняя: ReadParams/ApplyGammaRamp
до конструктора карты. Проверены clean upstream pins, игровые/frozen hashes,
основная/cumulative и E586 NRO, D:/logs2.7z. Commit/push и работа с устройством
не выполнялись. Цель порта остаётся активной.

## Воспроизведение

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_result_checks.py windows
python tools/matrixgame_result_checks.py policy-windows
python tools/matrixgame_result_checks.py policy-host
python tools/matrixgame_result_checks.py policy-arm64
python tools/matrixgame_result_checks.py sdk
python tools/matrixgame_result_audit.py --output reports/matrixgame-e2e589-original-result-recheck.json
```

Audit создаёт новый JSON exclusively. Полные compiler/link/package/runtime
команды и fingerprints записаны в evidence. Baseline уже завершён и сохранён;
для актуальной проверки его повторять не требуется.
