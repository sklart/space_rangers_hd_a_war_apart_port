# E2E-5.15: подготовка CPU/GPU/RAM диагностики

**Новый аппаратный инцидент:** после выполнения инструкции изменения logging
параметров HOC и перезагрузки пользователь сообщил об отказе загрузки Atmosphère.
Он загрузился в безопасном режиме и откатывает параметры. Причина ещё неизвестна,
успешная обычная загрузка после отката пока не подтверждена. Инструкция настройки
HOC отозвана; предыдущая проверка исходников не являлась аппаратной валидацией.
Сначала восстановление известной рабочей конфигурации, затем только игровые
perf515 журналы. Внешнее логирование/новые эксперименты HOC не повторять.

20.10.2026. **PREPARATION PASS / HARDWARE PENDING**. Дополнение принято как ТЗ.
Настоящий аппаратный прогон и причины тормозов ещё не установлены. MatrixGame
в объём дополнения не включён, push не выполнялся.

## Артефакты

Комплект: `build/e2e515/hardware-perf515-6c9b314.zip`, 26877331 байт,
SHA-256 `da93a207a6b8eef9b506ff681d51eeee66fe8472ec6a00ed13653dbc9d3bcfad`.
В нём папка switch для установки, matching ELF в debug, анализатор, mode-examples,
README, SHA256SUMS, source hashes, effective manifest/map и build report.

Исходный Git SHA NRO:
`6c9b3148d5f7609eba6ba2e5edcca51cb25c6794`.
Маркер `build_git=6c9b314-dirty`; dirty вызван прежним untracked отчётом,
tracked source tree перед сборкой чистый. После сборки проверено отсутствие
изменений compiled-source paths относительно этого SHA. Последующие commits
анализатора/упаковки не изменяют игровой код. Packaging Git SHA:
`8319cecd057c4af112af6f96d4f010e4b0838042`.

Build ID обоих файлов: `89e9db2998974a1b27bab1626cb216e6b8efffff`.

| Файл в комплекте | Байты | SHA-256 |
|---|---:|---|
| switch/space-rangers-hd-a-war-apart/SpaceRangersHD-perf515.nro | 16558384 | `adbfa32e4b25aeb8820c210b41ff7a3b2e73af75037b8042cfa2f4a272649d3e` |
| debug/SpaceRangersHD-perf515.elf | 56393224 | `065bf90ac53de0f54e5bd20ede5a59900f5bcc886d89c23fc1a97ea3c71a4569` |

Профиль Release O2, Save ON (RSG64/v168), Audio ON, E2E3 trace/scene PPM OFF.
Полные 298 игровых единиц, 381 объект пересобран с нуля, 380 link objects;
один оригинальный EC_HsFile заменён существующим native file backend.
Undefined=0, Win32 UNKNOWN=0, preflight/link/audits PASS. Матрица imports
не расширилась: 254 imports / 45 dynamic calls. Сохранены hashes 1422 входов.
Проверены perf515 scopes/frame/backlog/Record и настоящие игровые/OSK символы;
экспериментальные MatrixGame/deko и вызовы изменения частот отсутствуют.

В NRO заменено только название hbmenu на «Space Rangers HD E2E515 diagnostics».
Его SHA отличается от рабочего NRO сборщика: все загружаемые text/ro/data
сегменты после упаковки **побайтно совпадают**, Build ID совпадает с ELF.
Проверка ZIP CRC, SHA256SUMS и исходных бинарников PASS. Первоначальный
аппаратный main ZIP и GPU smoke ZIP дополнительно перепроверены, неизменны.
Восемь исходных файлов сейвов/архивов совпадают с data baseline.

## Изменения и проверки

- `e2e515_perf.cpp/.hpp`: ограниченная очередь и выделенный writer, runtime
  basic/detail/off, wall/own-thread CPU spans, кадры/тики/backlog, heartbeat,
  epoch anchors и RAM/Title ID/режим собственного процесса. Никаких вызовов
  изменения частот. Диагностические файловые операции выполняет writer.
- `e2e_stage.hpp`: в этой NRO сообщения публикуются только в очередь.
  `GR_Main` SessionLog redirected через отдельный checked overlay.
- `e2e515_perf_overrides.py`: только наблюдение реальных методов аркады,
  событий взрыва, ввода и границ боя; исходная логика и RNG не заменяются.
- `renderer_platform.cpp`: frame marker после успешного SDL Present.
  `e2e5_stage_profiler.hpp`: общий монотонный e2e_clock, экспорт этапов.
- `e2e_arm64_build.py`: отдельные output/manifest/flags без изменения готовой
  основной NRO; performance профиль требует Save ON и --no-e2e3-trace.
- `e2e515_perf_analyze.py`: чтение game/sys-clk CSV/log, bounded epoch/Title ID
  correlation, раздельные заданные/фактические частоты, raw поля неизвестного
  варианта, статистика/графики и предупреждения о качестве данных.

Host ASan/UBSan/leak: detail/off PASS. Test linker перехватывает fwrite/fflush
и проверяет, что их не вызывает producer. Проверены scopes, heartbeat,
переполнение с явными dropped, отсутствие performance samples в off и Shutdown.
Это проверки самого сборщика данных, не реальный игровой бой и не Switch.

Анализатор: **9/9 controlled tests PASS**, включая реальные имена колонок
оригинального sys-clk, разные requested/real values, чужой Title ID, clock jump,
неизвестные единицы fork, повреждённые строки, input immutability и графики.
График контролируемого примера визуально проверен; числа этого примера не
являются FPS игры. Python AST / git diff --check PASS.

Первый preflight диагностического графа выявил ожидаемый stale manifest
(новый путь и два файла). Локальная manifest обновлена, затем проверка PASS;
checked manifest обычной сборки не подменялась. Первая упаковочная symbol
проверка ошибочно требовала out-of-line CpuMicros, встроенный Release-компилятором;
исправлена проверка retained scopes + exact source hashes. Бинарник не менялся.

## Что передать после Switch

Порядок установки, basic/detail/off, сценарии A/B/C и команда анализатора:
[полная инструкция](../../docs/e2e515-performance-diagnostics.md).
Скопировать после каждого прогона `logs/perf515.csv` и `logs/perf515-stage.log`,
имеющиеся context.csv/log.txt модуля частот, сведения о режиме/питании и видео
обычного боя с проблемным моментом. Исходные сейвы сохранять; данные в Git
не добавлять. Основной приоритет — постоянная медленная работа всей аркады.

По фото известна включённая подпись **hoc-clk**, но точный fork/version и CSV
установки не подтверждены. На доступном G: соответствующие telemetry файлы
не найдены. Настройки оригинального sys-clk не применяются к hoc-clk автоматически.
Официальные источники формата и ограничения приведены в инструкции.

Последующее уточнение пользователя: **HOC 2.6.0**. Проверены исходники этого
тега: CONFIG_DIR=horizon-oc, целевой модуль hoc-clk 2.6.0. Нужные файлы находятся
в `/config/horizon-oc/`, а поля CSV совместимы с готовым анализатором. Инструкция
исправлена без изменения NRO/ELF и их архивов. Реальные файлы консоли не получены.

## Остаток приёмки

| Требование | Статус |
|---|---|
| Диагностическая NRO/ELF и скрипт анализа | PASS software preparation |
| Реальная запись game telemetry на Switch | HARDWARE PENDING |
| Версия/формат hoc-clk | HOC 2.6.0 сообщена пользователем; формат исходников проверен, фактический CSV установки pending |
| Корреляция с настоящим CSV этого прогона | HARDWARE PENDING; controlled parser PASS не закрывает её |
| Один воспроизводимый аркадный бой | HARDWARE PENDING |
| Наиболее затратный участок и причина | UNDETERMINED до аппаратных данных |
| Исправление подтверждённого дефекта и повторное сравнение | Следующий этап после результатов |

Объективные GPU timestamps/utilization не добавлены: частоты sys-clk сами по
себе не означают GPU bottleneck. Подробные AI/physics/collision scopes вложены
в Advance/TimerTakt; их нельзя суммировать. Цикл и физика сохранены, но измерения
имеют накладные расходы. Размеры, dropped/truncated, SD errors и clock alignment
оцениваются отдельно от реальной задержки игры.
