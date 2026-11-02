# E2E-5.64 — удержание gamepad X и оригинальный robot mode

26.10.2026. **ORIGINAL WINDOWS REGRESSION FIX PASS; NATIVE HID HARDWARE NOT RUN.**

Исправлен игровой дефект native input: общий Router typematic повторял также
виртуальные клавиши gamepad. X назначен на исходный KA_UNIT_ENTER; его удержание
повторно вызывало `CFormMatrixGame::Keyboard`, переключая управление роботом.
[Evidence](matrixgame-e2e564-pad-repeat-evidence.json).

## Воспроизведение и исправление

До изменения Router адресный ASan/UBSan test получил семь KeyDown от одного
удерживаемого pad key. Затем сохранённый старый Router скомпилирован отдельно
с настоящей Windows HAL игрой: original Init → PrepareMap → выбор реального
робота → gamepad-source KA_UNIT_ENTER. Через **500 мс / 20 кадров** original
Keyboard вывел игрока из выбранного робота. Baseline завершился exit 10 с
точным `holding gamepad X repeated original robot-mode toggle`; это не
подстановка фиктивного robot-mode state в CPU fixture.

Typematic теперь принадлежит физической Keyboard source. Pad вызывает key
edge и сохраняет held bit до отпускания, без повторных toggle/menu команд.
Original per-frame polling движения/стрельбы продолжает видеть held state.
Объединение keys/buttons, mouse aliases и исходная игровая логика не изменены.

У keyboard свой момент нажатия: если она присоединяется к уже удерживаемому
pad key, её delay начинается с физического keyboard press. Отпускание keyboard
останавливает её repeat, сохраняя pad-owned key. Присоединение/отпускание pad
не меняет keyboard timer. Сохранены 500 мс / 33 мс и ограничение catch-up до
четырёх событий. Модификаторы и focus/reset ведут себя по прежнему контракту.

Изменены только Router и комментарий его API. Native Switch HID polling,
раскладка контроллера и исходные CFormMatrixGame/Input методы сохранены.
Обязательные действия не заменены пустыми обработчиками.

## Проверки

| Gate | Результат и предел |
| --- | --- |
| Baseline CPU | PASS: повторные pad KeyDown воспроизведены под ASan/UBSan. Старые cpp/hpp/test bytes и executable сохранены отдельно. |
| Baseline actual game | Ожидаемый FAIL: exit 10 именно после unexpected mode change на 500 мс. Все compile/link шаги успешны. Старый Router взят из immutable baseline source snapshot, остальные проверяемые модули — из текущего workspace. |
| HOST ASan/UBSan | PASS: pad edge-only, held bits, keyboard timing/catch-up, keyboard join/repress на удерживаемой pad клавише, независимое владение, modifiers/focus. Это dispatch fixture, без HID/GPU. |
| Linux ARM64/QEMU | PASS: те же адресные проверки Router. |
| Existing original Form/Input regression | PASS на host ASan/UBSan и ARM64/QEMU: реальные original Form/Input dispatch, cursor ownership/OOM, keyboard repeat, mouse double-click, три mouse-key aliases, release orders и focus. Это CPU fixture, не libnx IPC. |
| Actual Windows HAL game candidate | PASS, две полные сессии. В каждой selected original robot retained при удержании pad X **800 мс / 32 кадра** и после отпускания. Последующие исходные movement/fire, mixed mouse/pad ownership, selection drag, focus release/recreation, frame loop и Deinit прошли. |
| SDK | PASS: devkitA64/libnx/deko3d compile/link и отдельная штатная NRO, strong undefined=0. |

Windows events контролируемые, проходят через production Router/EngineSink и
настоящие оригинальные Keyboard/MouseKey/Takt/Draw на настоящей карте. Это
не OS HID injection и не проверка физического контроллера Switch. Windows
selection probe добавлен условно; прежний обычный сценарий без нового define
сохранён. Полные map tests повторены именно для нового воспроизведённого
игрового дефекта, остальные большие suites не повторялись.

В capture/replay runner добавлены явные `--selection-source` и `--input-source`:
они позволяют собрать один и тот же actual game scenario со старым snapshot
или текущим Router, сохраняя отдельные baseline/candidate outputs. Конкретные
source paths и hashes записаны в command/result files. Input subset — единственная
изменённая production логика между этими двумя игровыми проверками.

## Отдельная NRO и воспроизведение

`build/matrixgame/e2e564-pad-repeat/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

Размер: **6 969 541 байт**. SHA-256:
`d34b1801af5f8e66c51df5540874d155ed2c30bc7c628fdf65bf8a03822724d8`.

Проверены три NRO segments, embedded icon/NACP и отсутствие обязательного
external RomFS. Стандартная цель — `matrixgame-native-diagnostic-nro`, штатные
nacptool/elf2nro. Все SDK/source/library inputs и точные commands сохранены.

```powershell
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_pad_repeat_checks.py host
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_pad_repeat_checks.py arm64
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_cursor_checks.py host --output build/matrixgame/e2e564-pad-repeat/input
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_cursor_checks.py arm64 --output build/matrixgame/e2e564-pad-repeat/input
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_pad_repeat_windows.py candidate
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_pad_repeat_sdk.py
```

Сохранённый baseline game воспроизводится через
`matrixgame_pad_repeat_windows.py baseline`, который читает прежний source
snapshot. CPU `baseline` mode предназначался для pre-fix source: current source
больше не должен воспроизводить старый отказ. Repro относится к текущему
подготовленному workspace с перечисленными local prerequisites, не к чистому
upstream checkout.

## Дополнительно проверенная граница звука

Оригинальный CSound использует Rangers callbacks и символьные имена из CFG,
например `Sound.FormShipClose`. Существующие `RangersBridge`/`EngineCallbacks`
дают полный 16-callback contract, но standalone native diagnostic не подключает
настоящие Rangers services. Отдельный SDL PCM output E2E4 сам по себе не
разрешает эти имена и не образует такой host. Неполная callback table или
выдуманное сопоставление WAV не устанавливались. Звук в native diagnostic
не объявляется интегрированным или проверенным; реальный host resource/audio
provider и его жизненный цикл остаются работой для полного порта.

## Точное состояние Init и оставшиеся gates

Последний реальный Switch journal всё ещё заканчивается в
`CGame::Init → g_Config.ReadParams → ApplyGammaRamp` до карты. Gamma исправлен
офлайн в E558; полный ReadParams проверен CPU в E559. После этого программно
добавлены bitmap loader, корректная single-sample state семантика, wireframe
selection и текущее исправление input. E564 Windows Init не является native
Switch Init PASS.

Открыты native Init/PrepareMap/first map frame, физический HID/focus/resume,
hardware gamma/image/VRAM и queue-loss recovery, настоящие Rangers callbacks
для звука/музыки и остальные ранее перечисленные renderer ограничения.
Отсутствие Switch ограничивает аппаратные gates, а не всю офлайн-разработку.

Generated original source files совпали с E563. Upstream clean на pin
218115a72fa9c5d43269f4ca84a8596a0bcb82f1. Основная NRO, прежняя E563 и ранее
зафиксированные NRO/dist/frozen/resource/save fingerprints, а также logs2.7z
сверены. Исходные ресурсы и saves не изменялись; outputs изолированы под E564.
Commit/push и взаимодействий с устройством не было.
