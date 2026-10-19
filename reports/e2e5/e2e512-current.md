# E2E-5.12 — текущая проверка объединённого `master`

Кандидат исходников: `c4dd5fb0c23b8ecac55a0024e4f19460f1944746`.
Это состояние после сверки старых worktree и целевых исправлений сохранений.
Отчёт [первоначальной проверки](e2e512-validation.md) относится к
`0f91c1a` и не описывает этот кандидат.

Все пять заданий [GitHub Actions run 39195287458](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/runs/39195287458)
прошли для указанного SHA. Обе пятёрки файлов скачаны из Actions и
проверены локально: ровно NRO, matching ELF, `build-report.json`,
`artifact-provenance.json`, `SHA256SUMS.txt`; SHA-256, Build ID, флаги
компиляции, `build_git` и `save_mode` совпали. Закрытые ресурсы, личные CFG,
сейвы и аппаратные ZIP в эти комплекты не входят.

| Режим | Actions artifact | NRO SHA-256 | ELF SHA-256 | Build ID |
| --- | --- | --- | --- | --- |
| Save OFF (`-DE2E5_DISABLE_DISK_SAVE=1`) | [12163741834](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/runs/39195287458/artifacts/12163741834) | `86fb47373fad11b18473c3faaf10942d4682511e22cbb0c4aea16f8795c6d8e4` | `ce4e8f40b3b45c57669dec3d0462f7e9be28b5499b1d92c2a76aee05a99a69ec` | `df8cc026dca5f61e1500e1e587cf14830d44aeea` |
| Save ON `save-test-v168` | [12163886291](https://github.com/sklart/space_rangers_hd_a_war_apart_port/actions/runs/39195287458/artifacts/12163886291) | `f5004d9c3384e609ccc8c01a190d4a63ed7b0519a95524182e7f481b2f43b723` | `1c2388dc8f0de0be534635dadb85f94a7e815459e2f4214fd5c55e6e999fc273` | `1bc4e00f2900b37b86facf3752d9201873be224c` |

Это `ARM64 BUILD PASS` и `CI PASS`, но ещё не `SWITCH PASS`.

| ID | HOST | ARM64/CI | SWITCH | Следующий критерий |
| --- | --- | --- | --- | --- |
| R-01 | preflight PASS | full link и CI PASS | — | Аппаратный игровой цикл остаётся отдельно. |
| R-02 | fault/transaction PARTIAL | sanitizer и ARM64 host PASS | PENDING | Полный reader/worker fault gate и реальная консоль. |
| R-03 | сценарий New Game PARTIAL | preflight PASS | PENDING для нового SHA | Реальная генерация и переход после `A`. |
| R-04 | UI PARTIAL | — | старый `0c90081`: дефект фона; новый SHA PENDING | Фон станции, новостей и магазина на новой NRO. |
| R-05 | один маршрут меню PASS | — | старый `0c90081`: зависание; новый SHA PENDING | Маршрут через медцентр и возврат в полёт. |
| R-06 | один маршрут Save ON PASS | Save ON link PASS | PENDING | Меню с сохранениями и миниатюрой карты. |
| R-07 | пять ходов, F5 и загрузка новым процессом PARTIAL | — | PENDING | Полный снимок мира и Switch round-trip. |
| R-08 | ручной UI слот PASS; отказ/ротация PARTIAL | sanitizer PASS | PENDING | Негативные случаи в игре и на Switch. |
| R-09 | arcade PARTIAL | — | PENDING | Трёхминутный бой, p50/p95/p99. |
| R-10 | input PARTIAL | — | PENDING | 10 свайпов и input-to-present. |
| R-11 | PARTIAL | — | PENDING | 5 сопоставимых ходов и world hash. |
| R-12 | PARTIAL | — | PENDING | 10 минут смешанного аудиомаршрута. |
| R-13 | PARTIAL | — | PENDING | Применение Options и восстановление CFG. |
| R-14 | — | — | PENDING | Два измеренных штатных профиля и батарея. |
| R-15 | MatrixLib/MG частичный build | CI native bridge PASS | OPEN | Настоящий планетарный бой и возврат результата. |
| R-16 | локальные отрицательные staging gates PASS | обе пятёрки CI PASS | — | Аппаратная проверка их поведения остаётся отдельной. |
| R-17 | — | — | PENDING | HOME, exit, restart, повторное сохранение. |

Доказательства host сохранений: [ручной слот](e2e512-save05-manual-host.md),
[повреждённые данные](e2e512-save06-corrupt-host.md),
[отказы записи и rollback](e2e512-save07-08-transaction-host.md),
[пять ходов и повторная загрузка](e2e512-save09-five-turn-host.md),
[Windows RSG/v167](e2e512-save10-windows-format-host.md).
Новый `QuickSave.sav` после пяти ходов: 783016 байт,
SHA-256 `8e60e835dac66a077b054d8ffbb4d6ea52d69cdf25ab478d5294d808f2497719`.
Его игровая и галактическая миниатюры декодированы как 300×225.
Работу миниатюры в меню Switch это не доказывает.

Аппаратный архив `D:/logs.zip` (SHA-256
`47a616a89aae807a9a36346d069ba6d1b67729968846e9caf8160adf84e79f1b`)
содержит только запуски `0f91c1a` и `0c90081`. Снимки из
`D:/screens.zip` (SHA-256
`b7db74b95826b7e7fb7bdc1d7cd1907041baf9d56f14b2916d0d6a88eb58a66e`)
показывают фон GameSettings за окнами станции/новостей. Это подтверждает
старый дефект, но не является проверкой текущей NRO. Перед последним обрывом
журнала `0c90081` событие `press_id=133` в `(47,680)` дошло до callback,
вложенный `OnOpen` завершился; следующего полного кадра или сообщения
об исключении нет. Причина зависания остаётся непроверенной.

Текущих Switch p50/p95/p99, before/after для расчётов и измерений батареи
нет. Публичного статуса `release-ready` нет. Следующий аппаратный маршрут —
[единый сценарий](../../docs/testing/switch-e2e512.md) после публикации
проверенной Save ON NRO. Пользовательские архивы, CFG и сейвы в commit
не включаются.
