# MatrixGame E604 — отдельные диагностические NRO

**Сборка, линковка и структура пакетов проверены. Запуск на Switch не выполнен:
HARDWARE_PENDING.** Основная NRO и все прежние диагностические пакеты сохранены.

| Файл | Назначение | SHA-256 |
| --- | --- | --- |
| matrixgame-native-interactive.nro | Ручной original native loop, до 120 секунд после Init | dadfc97e080604479bb3b71fcf9537212332a625798216e03ffb634743694cd2 |
| matrixgame-native-diagnostic.nro | Четыре подтверждённых видимых кадра карты | 3509f79247f0489efe4d49b229f8b29c9911815f92db383470418aa54d472525 |

Обе NRO имеют размер 14907589 bytes. Сборка находится в
`build/matrixgame/e2e604-current-diagnostic/1`; варианты — в отдельных
`interactive-nro/` и `diagnostic-nro/`.

Это обычные libnx/deko3d SDK-приложения. Текущая точка входа использует
настоящие Rangers services, original CGame::Init/Draw/SaveResult/cleanup и
native graphics/input providers. Она **не вызывает** Galaxy/Gov OnOpen и
интегрированный Robot::FRun из галактического экрана. Добавленный архив E603
передаётся линкеру целиком как dependency, но неиспользуемые функции могут
удаляться `--gc-sections`; наличие исправленного Gov в исходном архиве не
является доказательством его исполнения этой NRO.

Автоматические приказы host battle fixture в NRO не включены. Награда,
история и возврат в галактику проверены отдельно на Windows после естественного
поражения. Естественная победа и native/deko переход из галактики ещё открыты.

## Ресурсы и вывод

Игровые ресурсы в пакет не входят и не изменялись. Приложение читает прежние
подготовленные пользователем каталоги:

- `sdmc:/switch/matrixgame-diag/game` — MatrixGame CFG/DATA, включая robots.pkg;
- `sdmc:/switch/matrixgame-diag/rangers` — Rangers CFG/DATA.

Можно передать два альтернативных resource-root аргумента. Логи, readback,
minimap output и приватный `rangers-user` создаются в отдельном каталоге запуска
под `sdmc:/switch/matrixgame-diag/logs`. Основные сохранения не используются.
Требование памяти прежнее: диагностический entry проверяет application mode.
Перенос и запуск пользователь выполняет вручную; агент с устройством не работал.

## Управление

Manual: правый стик — указатель, A/B — левый/правый клик, D-pad — прокрутка,
L/R — поворот, X — вход/выход робота, ZR — огонь, ZL — Shift, Y — отмена,
Plus — оригинальное меню. **Minus+Plus — безопасное завершение диагностики.**
В quick режиме B или Plus отменяют проверку. Отмена — exit20, не PASS боя.
Оба режима имеют кооперативные тайм-ауты; они не обходят блокирующие SDK вызовы.

## Воспроизводимая сборка

```text
python tools/matrixgame_current_diagnostic_sdk.py
```

Используется новый attempt directory и стандартные цели:

```text
cmake --build <new-build-dir> --target matrixgame-native-interactive-nro matrixgame-native-diagnostic-nro -j 4
```

Точные configure/build команды, flags, SHA входов и артефактов сохранены в
`build/matrixgame/e2e604-current-diagnostic/1/result.json` и
`native-diagnostic-build.json`. Проверены AArch64 ELF, отсутствие strong
undefined symbols, original Init/Draw/host symbols, встроенные шрифты,
NRO segments, ASET/icon/NACP и отсутствие внешней RomFS зависимости.
Новые NRO не считаются доказательством аппаратной работоспособности.
