# E2E-5.93: исходный FRun и граница восстановления renderer

**PARTIAL по возврату в Galaxy.** Этап проверяет настоящий `Robot::FRun`
на Windows HAL с реальным базовым GI receiver. Это не `TfGov::OnOpen`, не
экран правительства и не галактическая сессия. Победа и native/deko FRun
остаются отдельными gates.
[Машинный evidence](matrixgame-e2e593-rangers-caller-reset-evidence.json).

## Исправленная ABI-граница

Исходный `Direct3D9::TD3DPresentParameters` хранит Win32 record: 56 байт,
DeviceWindow по offset 28, Windowed по offset 32. Windows x64 SDK использует
64 байта, HWND по offset 32, Windowed по offset 40. Передавать оригинальный
record прямо в SDK Reset нельзя.

`rangers_legacy_presentation.hpp` сохраняет исходный record, а Windows TU
переводит каждое поле в настоящий SDK D3DPRESENT_PARAMETERS, вызывает
IDirect3DDevice9::Reset и копирует изменённые поля обратно в 56 байт. HRESULT
не подменяется. Невыровненный input читается/пишется через memcpy. Генератор
`matrixgame_rangers_presentation_overrides.py` меняет только inline Reset
для Windows x64 в отдельной копии; upstream и исходные global layouts целы.

Для portable Rangers caller добавлен `RangersLegacyWindowBinding`: он связывает
оконный token с живым borrowed HWND. При активном binding чужой token
отклоняется до SDK call; полный HWND не сужается до token. SDK in/out сохраняет
исходный token. Вложенное владение запрещено, освобождение scoped.

Стандартная CMake target `matrixgame-native-rangers-interface` включает новую
SDK TU только при WIN32. На Switch этот Windows adapter не используется.
Перенос самого FRun renderer boundary на native/deko ещё предстоит.

## Отдельный настоящий Reset

MSVC ASan исполнил именно сгенерированный исходный inline entry:

- настоящий HAL Reset на 800×600 и SDK swap-chain readback всех полей;
- canaries вокруг невыровненных 56 байт остались целы;
- удерживаемый DEFAULT render target дал D3DERR_INVALIDCALL `8876086c`;
- освобождение target позволило успешный повтор на 640×480;
- null input/device отвергнуты;
- token `0xdeadbeef` разрешён через реальный HWND и сохранён при copyback;
- чужой token не изменил input, duplicate binding отвергнут, scope release
  вернул обычный reference путь.

## Настоящий caller и сохранённые попытки

FRun не извлекался в тестовую копию и не переписывался: используется прежний
оригинальный Robot.obj. GR_Main сохраняет прежний port-generated body, включая
исходный GR_DXReset; к его private platform state добавлена только test fixture
для настоящего SDL_CreateWindowFrom над тем же SDK окном. RegisterMainWindow
создаёт реальный portable window token. Чужое окно wrapper не уничтожает.

Receiver — оригинальный TMessageLoopGI с InitializeDefaults: настоящие panels,
label, cursor и update rectangles. Для label создан настоящий 565 pixel format
тем же способом, что в GR_DXInit. Это подготовка зависимостей diagnostic caller,
не полная реализация galactic renderer/UI session. Custom cursor отключён;
custom-cursor image restoration этим этапом не доказывается.

Сохранены все предыдущие попытки в отдельных каталогах:

- `frun-windows-asan`: ранний null CurrentPixelFormat при создании GI label;
  map локализовал PackNormalizedRgb → TLabelGI_Create → InitializeDefaults.
- `frun-windows-asan-fixed`: GI dependency исправлена, затем обнаружен неверный
  тестовый input: пустая строка карты отличается от nullptr/default map.
- `frun-windows-asan-map`: явная исходная desert1_3e.cmap исполнила полный бой
  до natural loss, но caller restore отказал из-за отсутствующего SDL window
  в private GR_Main platform state. Map локализовал ShowAndFocusMainWindow.
- `frun-windows-asan-sdl`: первый полный прогон с настоящим SDL foreign wrapper
  и явным token→HWND binding; runtime не принят, baseline сохранён.
- `reset-only`: исходный GR_DXReset с тем же настоящим SDL wrapper и 800×600
  SDK readback прошёл отдельно, без исполнения боя/FRun.
- `frun-windows-asan-diagnostic`: исходный FRun вернул code 2 и все шесть
  совпавших статистик, volumes/cwd/GI invalidation PASS. Внешний SDK owner
  возвратил 12 на смешанном gamma/leases oracle, поэтому весь runtime не PASS.
- `frun-windows-asan-gamma`: финальная проверка записывает gamma настроенного
  owning caller до боя и сравнивает SDK readback сразу после FRun при живом
  SDL owner. Сам исходный FRun/GR_DXReset этим исправлением теста не менялся.

Debugger run-1 остановился на first-chance ASan initialization; это не источник
диагноза игрового падения. Диагноз первого отказа получен сопоставлением
сохранённого runtime stack RVA с linker map. Для второй карты сделан map-only
relink без повтора большого боя.

## Текущее состояние проверок

Финальный `frun-windows-asan-gamma/result.json`: **PASS, runtime exit 0**.
Исходный FRun вернул code 2, signed STAT_TIME 116200, 10 robot kills,
5 turret kills. Все шесть полей original RobotBattleStatistics совпали с
независимым original SaveResult snapshot при естественном диалоге поражения.
82528 дополнительных Takt(25), map time 2073625 мс: ускоренная симуляция,
не realtime/FPS доказательство. Победа не подставлялась и не достигнута.

Sound/music volumes восстановлены; original GI invalidation содержит 800×600;
cwd и LooseFileRoot очищены штатной finally-веткой FRun. Настоящий SDK readback
gamma настроенного owning caller до боя совпал с readback после FRun при
живом SDL owner. После cleanup проверены target/viewport 800×600, отсутствие
depth surface и device/timer leases. Исходный FinalizeRobotRuntime вызван,
callback/service globals и host/map owners освобождены. Новые screenshot
image oracles этим этапом не запускались.

MSVC ASan частичный: новые adapter/fixture и GR_Main TUs инструментированы;
большие исходные archives/Robot.obj переиспользованы. Это не full-engine
ASan/UBSan/LSan и не native/deko runtime доказательство.

CMake devkitA64 configure/graph PASS: Windows TU отсутствует в
native interface graph, generated originals равны E592. Это configure-only,
не новая ARM64 compile/link/runtime проверка. Большие E590/E591/E592 тесты
повторно не запускались.
Новый полный бой запускается для нового FRun caller path, а не для повторения
уже доказанного standalone Run. Новая NRO не создаётся; E590 quick/manual и
E592 API ELF остаются прежними артефактами.

Итоговый audit сверил source/header/object/link/binary hashes, неизменность
protected game/frozen inputs, logs2, primary/root NRO, E590 пакетов и E592 SDK
артефактов; закреплённые upstream чисты. Whitespace и Python syntax checks PASS.

Воспроизведение с существующими проверенными E590/E592 dependencies:

```text
python tools/matrixgame_rangers_legacy_reset_windows.py
python tools/matrixgame_rangers_frun_windows.py
python tools/matrixgame_rangers_reset_sdk_configure.py
python tools/matrixgame_rangers_frun_audit.py --output reports/matrixgame-e2e593-rangers-caller-reset-evidence-new.json
```

Достигнутая цепочка: настоящий standalone Rangers data/audio/music host →
complete native RobotInterface → original FRun preparation и настройки →
original Init/бой/SaveResult/Deinit → caller result → original GR_DXReset через
Win64 ABI conversion и явный window identity → gamma/volumes/cursor policy/
GI invalidation/cwd cleanup → original FinalizeRobotRuntime. Базовый GI
receiver создан исходными классами; правительственный экран этим не заменён.

Открыты настоящий Galaxy/Player/Gov GI-tree, TfGov::OnOpen rewards/history и
галактический переход, естественная победа, native/deko FRun и пользовательская
Switch проверка. Сохранения не загружались и не изменялись; commit/push нет.
