# MatrixGame E586: управляемая диагностическая NRO

Актуальная отдельная сборка с SaveResult и поддержкой штатной сдачи:
[E589](matrixgame-e2e589-original-result.md). Ниже сохранены данные E586;
её бинарники не изменялись.

Для будущего ручного теста подготовлены две отдельные обычные SDK-сборки.
**Обе на Switch ещё не запускались.** Основная и прежняя E583 NRO сохранены.

| Режим | Бинарник | SHA256 |
| --- | --- | --- |
| Ручная проверка | `build/matrixgame/e2e586-interactive-diagnostic/sdk/interactive-nro/matrixgame-native-interactive.nro` | `3ebdf7e7217d1c9e110a9994663196614ee3228a0957a18e76bd4a91cc7aa713` |
| Быстрая проверка | `build/matrixgame/e2e586-interactive-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro` | `3ee1b00f60940c83b2b638a6a217201027e45f8b42fb90b7501871de27e80928` |

Manual — **14899397 bytes**, quick — **14895301 bytes**.
Общий source build ID `82034333146d-a22466c765f9114a90f0`; это идентификатор
семейства исходников, не SHA бинарника. В меню приложения различаются как
`MatrixGame diagnostic manual` и `MatrixGame diagnostic`.

Ресурсные roots, private outputs и read-only 26-entry manifest прежние:
[инструкция E575](matrixgame-e2e575-hosted-diagnostic.md). Дополнительные
ресурсы/сохранения не нужны. NRO содержит icon/NACP/fonts и не требует внешнего
RomFS. Данные и бинарники агент на устройство не переносил.

## Ручная проверка

`matrixgame-native-interactive.nro` проходит прежние smoke/Init stages, затем
оставляет оригинальный form/loop под управлением пользователя. Автоостановки
после четырёх кадров нет. После входа в loop начинается отдельный лимит
**120 секунд монотонного реального времени**; Init имеет собственный прежний
лимит 120 секунд. Проверка просроченного Init не обходится сменой фазы.

Штатные controller bindings из текущего native input:

- Правый стик — указатель; A — левый клик; B — правый клик.
- D-pad — прокрутка карты; L/R — поворот камеры.
- X — configured KA_UNIT_ENTER, вход/выход управления выбранным роботом.
- ZR — configured KA_FIRE; ZL — Shift; Y — отмена приказа.
- Plus — Escape/меню оригинальной игры.
- **Minus + Plus вместе — безопасное завершение диагностики.** Если сочетание
  удерживалось при запуске, сначала отпустить и нажать заново.

Можно закрыть стартовый диалог штатной кнопкой, выбрать робота, проверить
движение, войти в ручное управление и проверить стрельбу. B и отдельный Plus
больше не перехватываются диагностикой в этом режиме. Никакого автоматического
выбора робота, телепорта, изменения HP или подставной стрельбы entry не делает.

Сочетание выхода даёт `process_exit=20` / `cancelled user_exit` после stack
cleanup — это ожидаемая пользовательская отмена. Если исходный loop вернулся
обычно, минимум четыре подтверждённых видимых кадра и допустимый original exit
state проверяются; его значение пишется в журнал. Это не автоматический PASS
полного боя или возврата результата основной игре.

Лимит времени проверяется в кооперативных checkpoint и не прерывает зависший
блокирующий SDK/GPU вызов. Превышение лимита остаётся явным Timeout, а не успехом.

## Быстрая проверка и данные журнала

`matrixgame-native-diagnostic.nro` по-прежнему автоматически заканчивает loop
после четырёх кадров, B/Plus отменяют проверку. Теперь loading-transition кадры
**не засчитываются** как кадры видимой карты в обоих режимах. Original transition
выполняется полностью; первый `original-first.bgra` заказывается после снятия
MMFLAG_TRANSITION и при готовых video resources.

В отдельном `logs/<run>/` сохраняются `run.jsonl`, обычные smoke readbacks,
первый map readback и private output. Полезные поля:

- `session_mode` и `exit_controls` различают режимы;
- `original_loading_presents` — Present до видимой карты;
- `original_loop_frames_presented` — подтверждённые видимые кадры;
- `original_loop_frames_presented_final` — точный итог при обычном или
  кооперативном пользовательском/app-exit выходе;
- `original_exit_state` — значение при обычном возврате loop.

В manual подробные GPU/memory counters пишутся для первых четырёх видимых
кадров и затем каждые 60, а не на каждом кадре. Существующая долговечная
фиксация начала/конца Draw сохранена.

Проверены Windows, host ASan/UBSan и ARM64/QEMU **для portable session policy**,
а также compile/link/package обеих NRO, strong undefined = 0. Это не запуск
native/deko Init и не физическая проверка ввода/изображения/звука.
[Полный отчёт, воспроизведение и границы E586](../reports/matrixgame-e2e586-interactive-diagnostic.md).
