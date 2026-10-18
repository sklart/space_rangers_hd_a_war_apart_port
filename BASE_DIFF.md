# E2E-5.12: исходное состояние основного дерева

Проверено 18.10.2026. `master`, локальный `origin/master` и удалённый
`refs/heads/master` указывают на `0f91c1ab48328f187e10f58682d318b2487176bd`.
Аппаратно испытывавшийся `0c90081` находится в отдельной ветке
`codex/script-vm-regression` и рабочем дереве `.worktrees/stabilization-20261018`.
Эти ревизии не объединялись и не считаются эквивалентными.
Между `0f91c1a` и `0c90081` — 204 изменённых файла, в том числе отдельный
трек `port/matrixgame/`. Полный список имён:
`build/e2e512-evidence/tested-baseline-name-status.txt`.

До правок в основном дереве было 26 изменённых отслеживаемых файлов и
25 новых файлов/каталогов, включая `saves/`. Полный исходный список,
бинарный patch, контрольные суммы 51 файла и их локальная резервная копия:
`build/e2e512-evidence/initial-status.txt`, `initial-tracked.patch`,
`initial-files.json`, `initial-files.zip`. Архив SHA-256:
`274c5324f090f79cf446fe680cc2e2a237a7a6fecec120ba44046786fd5fe6e0`.
Эти файлы остаются локальными и не предназначены для GitHub Actions.

| Решение | Источник и пути | Основание |
| --- | --- | --- |
| Принято в основном дереве, ещё требовало проверки | `port/switch/platform/input_platform.*`, `win32_compat_messages.cpp`, `renderer_platform.cpp`, `e2e511_arcade_commands.hpp`; `tools/e2e511_overrides.py`, `e2e_source_overrides.py`, `e2e5_host_game_main.cpp`, `e2e5_host_game_probe.py` | Локальная работа E2E-5.11; эффект на Switch после этих правок ещё не проверен. |
| Принято в основном дереве, ещё требовало проверки | `port/switch/platform/package.*`, `e2e510_heap_reserve.hpp`, `e2e5_memory_recovery.hpp`, `e2e4_script_value.hpp`, `e2e3_generation_state.hpp`; `tools/e2e510_overrides.py`, `e2e4_script_width_overrides.py`, `e2e59_pointer_audit.py` | Выборочно перенесённые исправления из поздней локальной базы. Нельзя приписывать им аппаратный PASS `0c90081`. |
| Уже есть локально | `tools/ci_stage_switch_artifacts.py`, `tests/test_ci_stage_switch_artifacts.py`, `reports/e2e5/e2e511-followup.md` | Существовали до начала E2E-5.12, но отсутствовали в опубликованном `master`. |
| Не вошло | `0c90081`: `port/matrixgame/`, `docs/matrixgame-switch-plan.md` и прочие изменения из списка 204 файлов | Отдельный трек MatrixGame; массовое слияние нарушило бы границы текущего блока. |
| Не вошло | `.worktrees/stabilization-20261018`: `.github/workflows/package-host.yml`, `e2e_stage.hpp`, `renderer_platform.cpp`, `e2e4_audio_on_overrides.py` и другие локальные изменения | Отдельная незавершённая работа; перенос без адресного сравнения рискован. |
| Не вошло | Изменения `.worktrees/e2e511-flight-input` | Уже представлены адресными изменениями основного дерева; дерево сохранено для чтения. |
| Защищено | `saves/`, `D:/logs.zip`, `D:/screens.zip`, `windows/`, существующие `.worktrees/` | Не изменялись и не предназначены для коммита или CI-артефакта. |

Pinned submodules: `upstream/cpp=57fa689c630193a66fdea6ca4c79a188814991cd`,
`upstream/matrixgame=218115a72fa9c5d43269f4ca84a8596a0bcb82f1`,
`upstream/okgf=c01aa7a168a6f1772541501074b7bba1b96550ef`.
Сравнение исходных diffstat рабочих деревьев сохранено в
`build/e2e512-evidence/*-diffstat.txt`. Сам факт наличия правки не означает
успешного теста, коммита или аппаратного исправления.
