# MatrixGame E590: исправления длинного боя

Новые отдельные обычные SDK NRO, **на Switch не запускались**:

| Режим | Путь | SHA256 |
| --- | --- | --- |
| Manual | `build/matrixgame/e2e590-completed-battle/sdk/interactive-nro/matrixgame-native-interactive.nro` | `9a9ee42b529272cc09b5458f7186218d5430aae78acee447643bf7e5cf325e32` |
| Quick | `build/matrixgame/e2e590-completed-battle/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro` | `ba7e57f5d322c72c34ab2399ee6d29eecae86456e9e70592034645e079786e72` |

Обе — 14899397 bytes; source-family build ID
`82034333146d-80decd88f8f50b99f4af`. Основная и прежние NRO сохранены.
Resource roots/private outputs, controls, deadlines и SaveResult telemetry
прежние: [E589](matrixgame-e2e589-original-result.md),
[основная manual-инструкция](matrixgame-e2e586-interactive-diagnostic.md).

Исправлены два реальных сбоя длинного боя: потеря старших битов callback-указателя
огнемёта и отказ original Rangers loader от целого исходного Sound.Flame
с legacy RIFF size. Ресурс не изменялся; звук не отключался.

Host Windows HAL дважды выполнил исходный бой до естественного поражения:
1707100 мс игровой симуляции, код 2, 11 уничтоженных роботов и 5 турелей,
исходная статистика/выход/cleanup. Это ускоренный host-сценарий: diagnostic
NRO не получает никаких автоматических приказов из этого теста. Manual
по-прежнему ограничена 120 секундами реального времени после Init.

Проверка глобальной RobotBattleStatistics не является исполнением Galaxy UI.
У current standalone host нет Galaxy/Player/government screen/Run dispatch;
переход и применение наград/истории в галактической части пока NOT RUN.
Победа и hardware image/audio/HID/F9/focus/recovery тоже не доказаны.
[Полный отчёт и воспроизведение](../reports/matrixgame-e2e590-completed-battle.md).
