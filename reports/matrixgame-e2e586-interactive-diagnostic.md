# MatrixGame E586: отдельная interactive цель и правильный frame gate

Статус: session policy Windows/ASan/ARM64 execution PASS; две стандартные
devkitA64/libnx/deko3d NRO compile/link/package PASS, strong undefined = 0.
**Switch NOT RUN.** Game algorithms/generated original sources не менялись.

## Изменённое поведение

В прежнем diagnostic entry четырёхкадровая автоостановка и глобальная отмена B/+
мешали будущей ручной проверке выбора/движения/стрельбы. Добавлена отдельная
цель `matrixgame-native-interactive-nro`, разделяющая те же настоящие native/
deko/Rangers libraries с прежней `matrixgame-native-diagnostic-nro`.

Interactive сохраняет B/right click и Plus/Escape для оригинальной игры;
диагностический выход — Minus+Plus. После Init loop получает собственные 120
секунд, не отменяя прежний Init deadline. Удержанный при запуске exit не
превращается в новое нажатие сменой фазы. Все операции game/device/resource
по-прежнему выполняются оригинальным кодом; новый controller их не эмулирует.

Найден и исправлен отдельный дефект критерия: старый entry считал любой Present
original form кадром карты. После E580 loading snapshot такая четвёрка могла
ещё показывать переход. Теперь counter и первый readback учитывают кадры
только при MMFLAG_VIDEO_RESOURCES_READY и без MMFLAG_TRANSITION. Оригинальные
Takt/Draw перехода не пропускаются. Counter устройства проверяется на обратный
ход и повторное использование прежних Present. Windows reference E579–E585
ранее отдельно завершал transition; его доказательства этим не изменяются.

Manual не имеет автоостановки по frame budget. Обычный возврат original loop
принимается после минимум четырёх видимых кадров и при exit state 0..3;
quick сохраняет прежнее требование state=0. Original state записывается,
но это не доказательство полного боя/parent return. Пользовательский/app exit
остаётся отменой с кодом 20, deadline — явной ошибкой. Final visible-frame metric
добавлен на обычный и кооперативный выход. Частота подробных manual counters
снижена: первые четыре видимых кадра, затем каждые 60.

## Проверки

| Gate | Подтверждённая область |
| --- | --- |
| Windows execution | Настоящие portable SessionControl/RunControl, оба режима, все 16 button combinations, held/release arming, phase deadline boundaries, минимум кадров/exit states, transition exclusion, counter reuse/backwards errors и throttle. PASS, CPU policy only. |
| Host ASan/UBSan | Тот же oracle целиком инструментирован, leak/halt-on-error options, actual execution PASS. Это не полный engine sanitizer run. |
| ARM64/QEMU | Тот же policy oracle реально выполнен на AArch64. PASS; не GPU/physical HID. |
| SDK interactive | Отдельный ELF/NRO/NACP/map, CGame::Init/DekoDevice/real owning Rangers host присутствуют; zero strong undefined, AArch64 ELF, package validation PASS. |
| SDK quick | Вторая реальная target build в общей SDK директории, отдельный ELF/NRO/NACP/map, zero strong undefined, AArch64 ELF/package PASS. Общие archives не пересобирались вторично без нужды. |
| Profiles/assets | Compile commands показывают MATRIXGAME_DIAGNOSTIC_MANUAL=1 только у interactive entry. Разные NACP titles и binary hashes; все пять исходных font payloads присутствуют в обоих ELF. |
| Protected inputs | Прежняя E583/основная/cumulative NRO, clean upstream pins, 78 game inputs, доступные frozen/save artifacts и logs2.7z сверены и сохранены. |
| Switch | NOT RUN / HARDWARE_PENDING. Никакого переноса/запуска на устройстве. |

Первая software-проверка успешно прошла до добавления финального metric.
Её предварительный evidence сохранён отдельно. Окончательный аудит после
incremental rebuild: [final evidence](matrixgame-e2e586-interactive-diagnostic-final-evidence.json).
В общем binary directory исправлена инвентаризация SDK builder: snapshot одной
цели исключает entry object другой цели, который она не линкует; общий финальный
inventory продолжает сверять обе. Actual build inputs не исключались ради PASS.

Shipping original generated manifest побайтно совпал с E583. Изменены только
native diagnostic session/entry, CMake target/package glue, metadata и адресные
tools/tests. Старые большие game/ASan/ARM/Windows suites не повторялись.

## Точные артефакты

Общий source-family build ID: `82034333146d-a22466c765f9114a90f0`.

| Artifact | Размер | SHA256 |
| --- | --- | --- |
| `build/matrixgame/e2e586-interactive-diagnostic/sdk/interactive-nro/matrixgame-native-interactive.nro` | 14899397 | `3ebdf7e7217d1c9e110a9994663196614ee3228a0957a18e76bd4a91cc7aa713` |
| `build/matrixgame/e2e586-interactive-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro` | 14895301 | `3ee1b00f60940c83b2b638a6a217201027e45f8b42fb90b7501871de27e80928` |

Прежняя E583 NRO сохранена:
`53db0a16a8f187fc8dd50a55be20e2e3bd3335d6f86e80c02c13e164a9664233`.
Инструкция будущего ручного теста и controller bindings:
[E586 manual](../docs/matrixgame-e2e586-interactive-diagnostic.md).

## Воспроизведение

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_diagnostic_session_checks.py windows
python tools/matrixgame_diagnostic_session_checks.py host
python tools/matrixgame_diagnostic_session_checks.py arm64
python tools/matrixgame_interactive_diagnostic_sdk.py
python tools/matrixgame_interactive_diagnostic_audit.py --output reports/matrixgame-e2e586-interactive-diagnostic-recheck.json
```

После configure, с теми же verified dependencies, стандартные цели доступны
обычным `cmake --build <binary-dir> --target matrixgame-native-interactive-nro`
и `--target matrixgame-native-diagnostic-nro`. Воспроизводимость exact compiler/
link/package commands и fingerprints записана в final evidence. Audit пишет
новый JSON exclusively. SDK packaging использует обычные nacptool/elf2nro;
основная NRO/её target не затронуты.

## Остаток

Программная цепочка Init/providers/host и новая политика собраны и проверены
в пределах указанных gates. Полный native/deko Init и реальные видимые map
frames ещё не выполнялись. Последняя измеренная Switch boundary из logs2 —
ReadParams/ApplyGammaRamp до map constructor — остаётся прежней. Нужны будущие
пользовательские hardware Init/image/audio/HID/F9/focus/recovery проверки и
полный бой/результат/возврат. Лимиты cooperative, blocking SDK call не прерывается
этим controller. Commit/push не выполнялись; цель порта не объявляется завершённой.
