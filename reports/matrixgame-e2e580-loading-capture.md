# MatrixGame E580: native снимок для оригинального перехода загрузки

Статус: production integration и адресные software checks PASS; отдельная
SDK NRO собрана. Полный native/deko Init на Switch **NOT RUN**.

## Реальная операция вместо GDI-границы

Оригинальный `CTransition::BuildTexture` теперь получает изображение через
`transition::CaptureLoadingImage`. Геометрия перехода, заполнение power-of-two
bitmap, BC1 encoding/upload, движение плиток, Takt и Draw остаются оригинальными.
Pinned upstream не изменён: адаптация применяется к generated port copies.

Перед `CGame::Init` native entry с Rangers services создаёт
`LoadingCaptureScope` из настоящего `ScreenshotProvider` устройства. `DekoDevice`
уже выполняет readback завершённого кадра с проверкой presentation proof,
синхронизацией очереди и чтением реального scene image. На этом этапе diagnostic
entry уже представил настоящий статусный кадр перед загрузкой. Захват сохраняет
собственные CPU bytes до оригинального video Reset, который инвалидирует GPU slot.

Поздний readback внутри BuildTexture после Reset был бы недопустим: завершённого
presentation proof уже нет. Scope выполняет readback один раз до Init. При
потреблении проверяются размер, формат, pitch и объём; пиксели копируются в
настоящий `CBitmap`, затем память снимка освобождается. Повторное потребление,
смена размеров или отсутствие завершённого кадра дают явную ошибку. Выдуманное
изображение, пропуск обязательного перехода и фиктивные GDI handles не используются.

Scope принадлежит runtime и снимается после original Deinit, в том числе при
ошибке Init. Отказ провайдера происходит до публикации binding. Старый Windows
reference без native scope продолжает настоящий GetDC/BitBlt/GetDIBits путь;
проверяется отказ основных операций. Общая RGBA/RGB565 → RGB24 конвертация
выделена из F9 screenshot без изменения оригинального PNG/CFile encoder.

## Исполненные проверки

| Gate | Доказательство |
| --- | --- |
| Windows CPU snapshot | RGBA/RGB565 с padded pitch, четыре format/cycle случая; owned bytes после изменения provider storage, one-shot, отсутствие scope, двойное binding, пять invalid/capture-failure вариантов и retry. PASS; synthetic pixels, NO GPU. |
| Host ASan/UBSan | Тот же oracle с инструментированными test/transition/screenshot/platform translation units; detect_leaks и halt_on_error. PASS. Reused Base/CBitmap зависимости имеют собственную прежнюю область проверки; full-engine sanitized execution не заявляется. |
| ARM64/QEMU | Тот же реальный CBitmap и snapshot lifecycle/oracle. PASS; CPU/ABI, без ARM ASan/GPU. |
| Windows HAL snapshot | Настоящий D3D9 HAL рисует два цветовых региона, Present и GetRenderTargetData дают снимок. Затем настоящий Reset меняет target с 64×48 на 80×60 и перерисовывает его. Старые pixels совпадают с независимым RGB oracle: 18432 channel comparisons за два цикла; one-shot и COM cleanup PASS. Это PC GPU proof, не deko queue proof. |
| Native Init failure/retry | Real owning Rangers host и production InitializeNativeGame: два отказа capture до CGame::Init, каждый с cleanup и последующим повторным входом до намеренно отказывающего ReadCaps. Real text/sound callbacks, runtime owners, copied table, output binding и финальная доступность capture scope проверены. PASS; synthetic CPU image, NO GPU/FULL INIT. |
| PNG/CFile regression | Windows, ASan/UBSan host и ARM64/QEMU: по 36 настоящих PNG; по 18 layout/format cases с повтором. Независимый stdlib decoder сверяет CRC, zlib, размеры и каждый RGB pixel всех 108 файлов. Private output и существующие файлы сохранены. |
| Windows full composition | Fresh production archives: 160 actual MSVC x64 objects, assertions включены. Два original Init/map/transition/4-visible-frames/Deinit цикла с настоящим Rangers host и D3D9 HAL. Восемь 800×600 readbacks, 147404/147288 distinct RGB colors; cleanup host, gamma/window и device/timer leases PASS. |
| devkitA64 | Отдельная стандартная цель matrixgame-native-diagnostic-nro, libnx/deko3d и настоящий Rangers host; AArch64 ELF link, strong undefined=0, NRO/ASET/NACP/fonts/source/dependency audit PASS. |
| Switch | NOT RUN / HARDWARE_PENDING. Устройство и SD не использовались. |

Сначала сохранённые результаты/dirty Git state проверены без reset, stash, clean
или удаления. Исчезнувший старый process handle не трактовался как ошибка
сборки: найден завершённый `native-build/build-result.json` с PASS. Первый
изолированный WSL запуск получил E_ACCESSDENIED; сохранён как отдельный неуспех,
проверка затем прошла через разрешённый установленный WSL. Автоматических
запретов approval review не было. Исправлены два пути самого аудитора (generated
OriginalTransition и WSL archive paths); соответствующие SDK fingerprints
обновлены. Это не повод повторять уже пройденные большие suites.

## Артефакты и воспроизведение

Новая NRO:
`build/matrixgame/e2e580-loading-capture/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.
Размер **14895301 bytes**, SHA256
`b6ec6ce859ce21c9471084d1e63be8a45d7f492f0f98a31e0b3cb421e4ee6c8b`,
build ID `82034333146d-881a0f68c6186ed89122`. Эти значения также в `sdk/result.json` и
[итоговом evidence](matrixgame-e2e580-loading-capture-evidence.json).
Предыдущая E579 NRO сохранена с SHA256
`020b6d3debff52d5ae1efb757b2aee10b6e9886692d83a2d2d8abac2cabf0474`.
Основная/cumulative NRO не изменены.

Команды из корня проекта с установленными VS, Python, WSL и devkitPro:

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_loading_capture_native_build.py
python tools/matrixgame_loading_capture_checks.py windows
python tools/matrixgame_loading_capture_checks.py windows --gpu
python tools/matrixgame_loading_capture_checks.py host
python tools/matrixgame_loading_capture_checks.py arm64
python tools/matrixgame_loading_capture_screenshot_checks.py windows
python tools/matrixgame_loading_capture_screenshot_checks.py host
python tools/matrixgame_loading_capture_screenshot_checks.py arm64
python tools/matrixgame_loading_capture_prefix_checks.py
python tools/matrixgame_loading_capture_map_checks.py
python tools/matrixgame_loading_capture_sdk.py
python tools/matrixgame_loading_capture_audit.py --output reports/matrixgame-e2e580-loading-capture-recheck.json
```

Audit создаёт новый файл exclusively. Exact compiler/link commands, source и
archive hashes, stdout и независимые pixel observations сохранены в evidence.
Новые wrapper scripts используют прежние проверенные зависимости read-only.
Для новой проверки не требуется пересобирать исторические E527/E529 suites.

## Точная Init-цепочка и остаток

E527 output-folder provider уже завершён: original CFile round-trip,
Unicode/traversal/error/ownership/unwind и Windows cold/store/warm minimap cache
прошли; отдельный private output root удерживается до конца original teardown.
См. [E527](matrixgame-e2e527-output.md). `StoredStateBackend` и CDeviceState
интегрированы и ранее проверены отдельно, см. [E529](matrixgame-e2e529-stored-state.md).
Они не подменяются новыми fixture implementations.

Программная композиция: owning Rangers data/packages/config → audio/music →
full16 callbacks → NativeInitRuntime/output/services → original CGame::Init →
video/stored state → original map/load/prepare → minimap/textures → loading
transition → original map frames → original Deinit → host cleanup. Полностью
эта цепочка исполнена на Windows HAL. Host/ARM64 подтверждают обозначенные CPU
префиксы и ownership; SDK подтверждает compile/link/package.

Native снимок для обязательной transition-ветки теперь подключён, прежний
известный программный пробел E579 устранён. При отсутствии завершённого кадра
или изменении его размеров действуют явные ошибки; resampling не придуман.
Полное совместное исполнение CGame::Init/map/frames именно через deko3d ещё не
доказано. Следующий фактически отказывающий native метод пока не установлен.
Последняя измеренная аппаратная граница из logs2 — ReadParams/ApplyGammaRamp
до map constructor — остаётся прежней. Нужны пользовательские Init/first-frame,
image/audio/HID/focus/F9/recovery проверки. Это **PARTIAL**, не игровой release PASS.

Проверены 78 защищённых game inputs, доступные frozen/save артефакты,
primary/cumulative/прежние NRO, logs2.7z и clean upstream pins. Ничего из них
не изменено; commit/push, перенос данных и аппаратный запуск не выполнялись.
