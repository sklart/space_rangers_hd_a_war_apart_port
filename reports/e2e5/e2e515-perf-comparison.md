# E2E-5.15: аркада — измерения host и открытые сравнения

20.10.2026. PARTIAL; это не итоговая оценка FPS Switch и не доказательство
ускорения относительно baseline. Три парных before/after series, полная
world/geometry/image parity 500/1000 и input latency ещё OPEN.

## Настоящая смерть и анимация

`tools/e2e515_arcade_probe.py` собирает отдельный диагностический ELF из
существующих 331 объектов полной host-игры, заменяя три единицы инструментированными
копиями: ab_MainForm, ab_Hit, renderer_platform. Остальные игровые функции,
карта и ресурсы настоящие. Pinned upstream и основная NRO не изменяются.
Смерть воспроизводится вызовом исходного `PlayerArcadeShip->ApplyDamage` на
тике 50; это контролируемое смертельное попадание, не естественное столкновение.
Исходные анимация, ExplosionComplete, countdown и DefeatPanel исполняются игрой.
System seed после LoadPanel::OnOpen: 0x515. Вход в арену выполняет существующий
legacy menu probe; controller-only вход/выход этот тест не доказывает.

Точка кадра — успешный host `PresentRgb565` после PresentFrame. В host ветка
`SDL_RenderPresent` не исполняется: физического показа/scanout/GPU здесь нет.
DrawFrame и завершённый software framebuffer считаются отдельно. Таймерные
анимации зависят от wall-time, поэтому номер тика завершения взрыва между
прогонами может различаться; это не основание объявлять одинаковое состояние мира.

Первый диагностический запуск `d0b7550e8d5e4dfda6877eb26e506b9a` использовал
недостижимую host SDL-точку, дал нулевой счётчик и был остановлен после проверки
точного PID/cwd; результат сохранён FAIL. Его метрики кадров не используются.

Корректный ELF SHA-256:
`9b0089751fd7568561c4d1204b4eb74db8532a69260bc6ba7dcce1826f23fce6`.
Source/header/generated SHA: `build/e2e515/arcade/provenance.json`.

| Сценарий | Завершённых host-кадров | Result tick | Explosion complete tick | Gap p50/p95/p99/max, мс |
|---|---:|---:|---:|---|
| Без искусственной задержки | 245 | 201 | 231 | 18.42 / 24.46 / 32.70 / 391.67 |
| +20 мс в каждом активном TimerTakt | 371 | 201 | 338 | 26.35 / 29.12 / 32.79 / 142.00 |
| +40 мс в каждом активном TimerTakt | 261 | 201 | 231 | 47.64 / 59.53 / 76.09 / 371.64 |
| +70 мс в каждом активном TimerTakt | 263 | 201 | 231 | 76.59 / 82.33 / 95.04 / 175.68 |
| +150 мс в каждом активном TimerTakt | 371 | 201 | 338 | 156.69 / 161.61 / 170.31 / 255.17 |
| +250 мс в каждом активном TimerTakt | 371 | 201 | 338 | 257.16 / 262.87 / 272.86 / 402.59 |

Все шесть строк — выполненные последовательные прогоны одного ELF, HOST SOFTWARE
LIVENESS PASS. Краткая машинная сводка со всеми run IDs:
`reports/e2e5/e2e515-arcade-overload.json`. Для каждого run исходный `result.json`
и сырой `arcade-events.csv` сохранены внутри `build/e2e515/arcade/runs/<run>/`.
Максимальный интервал каждого прогона ниже `2 с + synthetic delay`; несколько
десятков секунд непрерывного голодания software presentation не воспроизведены.
На +250 мс result представлен на 52.694 с, ExplosionComplete пришёл на 88.091 с:
планировщик уступает между callbacks, но ход и анимация сильно растягиваются.
Это конкретное оставшееся ограничение CPU-overload, не real-time performance PASS.

Данные первого корректного прогона:
`build/e2e515/arcade/runs/9f1006f138394df7badaae336058ef43/`:
`result.json`, `arcade-events.csv`, `stderr.log`.
Измеренное окно 5.566 с, main-thread CPU 1.633 с; tick CPU 0.185 с,
draw CPU 1.368 с. p50/p95/p99 tick CPU 0.490/1.162/1.969 мс;
draw CPU 0.779/2.800/3.165 мс. Это один прогон с диагностическими flush,
загрузкой эффектов и подготовкой ресурсов, не paired optimization comparison.
Максимальный интервал относится ко всему окну, не только к взрыву.

Прогон +40 мс: `build/e2e515/arcade/runs/ae78a86af7ea44bc859514f3f4d84e0f/`.
Окно 14.250 с, main-thread CPU 1.144 с. После смертельного попадания панель
поражения дошла до host framebuffer за 7.686 с: это замедление при перегрузке,
а не обещание одновременно сохранить real-time и все тики.
Хеш выбранных полей на тике 231 отличается: без задержки
`be2cffc1a8ab4eb3`/22 объекта, +40 мс `02fe0c401cd24068`/24 объекта.
Поэтому эти death-прогоны подтверждают только liveness/исходные callbacks;
**world/RNG parity между ними не объявляется PASS**. Перед оптимизациями
нужен отдельный детерминированный replay и исследование зависимости от времени.
Process-wide CPU всех worker threads, RSS/heap и input-to-action пока не измерены.
Пиксели самого спрайта взрыва между кадрами пока не сравнивались: продолжение
software presentation и завершение callback не подменяют этот отдельный gate.
Игровой watchdog со stack/stage/input latency также ещё OPEN.

Процесс останавливается `_Exit(0)` после показанного software result и callback
окончания взрыва. Нормальный shutdown, очистка ресурсов, death→retry,
result→main, pause/resume и новый запуск этим не проверены.
Checkpoint — хеш выбранных scalar-полей объектов/health и form/System RNG,
а не полная сериализация мира. Геометрические и image checkpoints ещё OPEN.
Исходный CFG копируется и проверяется по SHA; пользовательские сейвы не пишутся.

## Воспроизведение

В Linux/WSL после `python3 tools/e2e515_engine_tests.py`:

```sh
python3 tools/e2e515_arcade_probe.py --build --death-tick 50 --game-root '/path/to/game' --config-root '/path/to/copied/config'
python3 tools/e2e515_arcade_probe.py --death-tick 50 --slow-ms 40 --game-root '/path/to/game' --config-root '/path/to/copied/config'
```

Новый уникальный каталог выводится как `Arena stage`. Доступны overload
20/40/70/150/250 мс и отдельные `--ticks 500`/`--ticks 1000` без lethal hit.
Доступность аргумента не означает выполненный тест. Общий timeout harness 420 с
считает незавершённый прогон FAIL; это не игровой watchdog и не исправление игры.
Runner также отвергает прогон без интервалов завершения кадра, с exception-marker
или max gap больше `2 с + synthetic delay`: достижение result само по себе не
может скрыть исходное многосекундное голодание.

На Switch остаются HARDWARE PENDING зависание около 40 с, p95/p99, скорость
игрового времени, SDL present, отзывчивость контроллера и handheld/docked.

## Обычный бой: аппаратный профиль и отклонённый эксперимент

После уточнения пользователя приоритет — постоянные тормоза в обычной аркаде;
взрыв остаётся крайним регрессионным случаем. Из сохранённого аппаратного лога
`build/e2e514/hardware-input/logs/e2e.log` пересчитаны 12 профильных окон:
1547 TimerTakt, взвешенное по числу вызовов среднее окон 36.367 мс.
Advance (stage 2) — 24.405 мс, worldline (stage 4) — 7.429 мс.
Это историческое измерение приставки, не новая hardware проверка; p95 всех
вызовов из агрегированных окон не восстанавливается. Нельзя объяснять эту
нагрузку только SDL present или взрывом.

В диагностическом обычном бое добавлены счётчики и CPU samples горячих функций,
геометрические checkpoints и RGB565 hashes на реально завершённых host кадрах.
Каждый 64-й вызов измеряется clock_gettime; полный call count сохраняется.
Inclusive samples вложены друг в друга, их нельзя суммировать в общий CPU.
Первоначальный all-calls profiler сам заметно замедлял проекции и не используется
для сравнения performance. Исходная идея повторных sin/cos отклонена до компиляции:
такая правка уже присутствует в effective ab_Global через e2e56_geometry_overrides.

Исследован отдельный opt-in `--variant polygon-memo`: ограниченная таблица
результатов оригинальной проекции полигонов с точными matrix/point/FP-control keys.
Point pointers и ресурсы не сохраняются, арифметика не упрощается, исходные
DrawFrame/AI/events не пропускаются. Проверка компонента на настоящем
ProjectPointByMatrix и настоящем runtime дала 98304 побайтовых сравнения в четырёх
режимах округления: ASan/UBSan PASS и ARM64/QEMU PASS. Проверены изменения
матрицы/координат, повторное использование адресов и исключение projector.
Отдельный реальный Switch polygon translation unit с кандидатом скомпилирован
devkitA64: COMPILE PASS, linked=false, hardware_executed=false.
Данные: `build/e2e515/polygon-contract/{asan,arm64,sdk}/result.json`.

Полный обычный бой без искусственных задержек и без lethal hit:

| 1000 активных тиков, seed=0x515 | Baseline | Polygon memo |
|---|---:|---:|
| Main-thread CPU, с | 7.795 | 10.296 |
| Timer CPU, с | 0.966 | 0.943 |
| Draw CPU, с | 6.405 | 8.894 |
| DrawFrame calls | 3123 | 5203 |
| Software presentations | 948 | 1098 |
| Среднее CPU одного DrawFrame, мс | 2.051 | 1.709 |

**REJECTED**: общий CPU вырос на 32.08%; world/RNG и геометрия на 500/1000 тиках
не совпали. Из 870 общих ticks полные framebuffer hashes совпали в нуле случаев.
Из-за различия состояния даже эти времена не являются валидной performance
победой. Три дополнительные пары не запускались: первый обязательный parity/total
gate уже FAIL. В shipping overrides кандидат не подключён, header перенесён в
`tests/`; он сохраняется только как воспроизводимый отвергнутый эксперимент.
Сводка: `reports/e2e5/e2e515-arcade-normal-profile.json`, сырые CSV/JSON по run IDs
внутри `build/e2e515/arcade/runs/`.

Установлено по коду: EndFramePresentation ограничивает только фактическую
презентацию; DrawFrame выполняет полную работу до этой проверки. В эксперименте
отдельный кадр стал дешевле, но число DrawFrame выросло. Отдельно в
SE_Space::TObjectSE::Advance случайные звуки зависят от timeGetTime и используют
System::RandSeed. Вклад этого wall-time пути в конкретное расхождение пока
НЕ ДОКАЗАН; это точная проверяемая гипотеза, а не установленная первопричина.
Следующий gate — baseline repeatability и одинаковый replay timer/input/render,
с фиксацией первого расхождения. Дальнейшее пропускание DrawFrame, изменение
точности и перенос RNG не внедрялись. Постоянные аппаратные тормоза ещё не
объявлены исправленными.

Воспроизведение контракта: `python3 tools/e2e515_polygon_tests.py`, затем
`python3 tools/e2e515_polygon_tests.py --arm64`; devkit compile на Windows:
`py -3 tools/e2e515_polygon_tests.py --sdk`.
Обычный бой: команда arcade_probe выше без `--death-tick`, с `--build --ticks 1000`
и `--variant baseline` либо `--variant polygon-memo`. Варианты собираются в
раздельные `build/e2e515/arcade/variants/<variant>/`.
